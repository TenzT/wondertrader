#include "WtStraGrid.h"

#include "../Includes/ICtaStraCtx.h"

#include "../Includes/WTSContractInfo.hpp"
#include "../Includes/WTSVariant.hpp"
#include "../Includes/WTSDataDef.hpp"
#include "../Share/decimal.h"
#include <cmath>

extern const char* FACT_NAME;

//By TenzT @2024.11.11
#include "../Share/fmtlib.h"

inline double average(std::vector<double> &array, size_t begin, size_t end) {
	if(begin <0 || begin >= array.size() || end < 0 || end > array.size())
		return INVALID_DOUBLE;

	double sum = 0;
	for (size_t i = begin; i <= end; ++i) {
		sum += array[i];
	}
	return sum / (end - begin + 1);
}

WtStraGrid::WtStraGrid(const char* id)
	: CtaStrategy(id)
{
}

WtStraGrid::~WtStraGrid()
{
}

const char* WtStraGrid::getFactName()
{
	return FACT_NAME;
}

const char* WtStraGrid::getName() {
	return "Grid";
}

bool WtStraGrid::init(WTSVariant* cfg)
{
	if (cfg == NULL)
		return false;

	_p1 = cfg->getDouble("p1");
	_p2 = cfg->getDouble("p2");
	_short_days = cfg->getUInt32("short_days");
	_long_days = cfg->getUInt32("long_days");
	_num_grids = cfg->getUInt32("num_grids");

	_period = cfg->getCString("period");
	_bar_count = cfg->getUInt32("bar_count");
	_code = cfg->getCString("code");

	_capital = cfg->getDouble("capital");
	_margin_rate = cfg->getDouble("margin_rate");
	_stop_loss = cfg->getDouble("stop_loss");

	_pricemode = 1;	// 先固定只能做多
	return true;
}

void WtStraGrid::on_schedule(ICtaStraCtx *ctx, uint32_t curDate, uint32_t curTime)
{
	// 读取日线数据以计算均线的金叉
	WTSKlineSlice *kline = ctx->stra_get_bars(_code.c_str(), "d1", _bar_count, false);
	WTSValueArray *closes = kline->extractData(KFT_CLOSE);
	double ma_short_days1 = average(closes->getDataRef(), closes->translateIdx(-_short_days), closes->translateIdx(-1));
	double ma_long_days1 = average(closes->getDataRef(), closes->translateIdx(-_long_days), closes->translateIdx(-1));
	double ma_short_days2 = average(closes->getDataRef(), closes->translateIdx(-_short_days), closes->translateIdx(-2));
	double ma_long_days2 = average(closes->getDataRef(), closes->translateIdx(-_long_days), closes->translateIdx(-2));
	closes->release();
	kline->release();	// 可以用RAII来自动释放

	// 读取的period线
	kline = ctx->stra_get_bars(_code.c_str(), _period.c_str(), _bar_count, true);

	double cur_price = ctx->stra_get_price(_code.c_str());

	uint32_t trdUnit = 1;
	double cur_pos = ctx->stra_get_position(_moncode.c_str());
	if (decimal::eq(cur_pos, 0))
	{
		// 当没有持仓时，用总盈亏加上初始资金计算出当前总资金
		_cur_money = ctx->stra_get_fund_data(0) + _capital;//
	}

	// 计算交易一手所选的期货所需的保证金
	double trdUnit_price = _vol_scale * cur_price * _margin_rate;

	if (decimal::eq(cur_pos, 0))
	{	
		if (decimal::gt(ma_short_days1, ma_long_days1) && decimal::gt(ma_long_days2, ma_short_days2)) 
		{
			// 半仓
			double qty = floor(_cur_money * _margin_rate * 0.5 / trdUnit_price);
			// 当前未持仓且日线金叉（快线超过慢线）时作为基准价格进场
			ctx->stra_enter_long(_moncode.c_str(), qty, "enterlong");
			// 记录进场时的基准价格
			_benchmark_price = ctx->stra_get_price(_code.c_str());
			// 记录grid_pos表示网格当前实际仓位
			_grid_pos = 0.5;
			ctx->stra_log_info(fmt::format("进场基准价格{:.2f}", _benchmark_price).c_str());
		} 
	}
	else 
	{
			double target_pos{0};
			
			double price_ratio = cur_price / _benchmark_price;

			ctx->stra_log_debug(fmt::format("price_ratio: {:.2f}", price_ratio).c_str());
			// 按价格网格计算目标仓位
			if (decimal::lt(price_ratio, _price_list.front())) 
			{
				// 低于网格满仓
				target_pos = 1;
				ctx->stra_log_info("低于网格最小上边界，满仓做多");
			}
			else if (decimal::gt(price_ratio, _price_list.back()))
			{
				// 高于网格平仓
				target_pos = 0;
				ctx->stra_log_info("高于网格超出最大上边界，全部平多");
			} 
			else 
			{
				// 按网格设置目标仓位
				for (size_t i = 0; i < _price_list.size()-1 ; ++i) 
				{
					if (decimal::le(_price_list[i], price_ratio)
						&& decimal::lt(price_ratio, _price_list[i+1])) 
					{
						if (decimal::lt(price_ratio, 1)) {
							target_pos = _position_list[i+1];
							break;
						} 
						else 
						{
							target_pos = _position_list[i];
							break;
						}
					}
				}
			}

			// 止损逻辑 当价格低于下边界乘以设定的stoploss比例时止损
			if (decimal::le(price_ratio, _price_list[0] * _stop_loss))
			{
				target_pos = 0;
				ctx->stra_exit_long(_moncode.c_str(), ctx->stra_get_position(_moncode.c_str()), "Exit long");
				ctx->stra_log_info(fmt::format("价格低于最大下边界*{:.2f}，止损，全部平多", _stop_loss).c_str());
				_grid_pos = target_pos;
			} 
			else if (decimal::gt(target_pos, _grid_pos)) 
			{
				// 加仓
				double qty = floor((target_pos - _grid_pos) * _cur_money / trdUnit_price);
				ctx->stra_enter_long(_moncode.c_str(), qty, "enterlong");
                ctx->stra_log_info(fmt::format("做多{:.2f}手,目标仓位{:.2f},当前仓位{:.2f},当前手数{:.2f}", 
												qty, target_pos, _grid_pos, ctx->stra_get_position(_moncode.c_str())).c_str());
                _grid_pos = target_pos;
			} 
			else if (decimal::gt(_grid_pos, target_pos)) 
			{
				// 减仓
				double qty = floor((_grid_pos - target_pos) * _cur_money / trdUnit_price);
				qty = qty < 0 ? 0 : qty;
				ctx->stra_exit_long(_moncode.c_str(), qty, "exitlong");
                ctx->stra_log_info(fmt::format("平多{:.2f}手,目标仓位{:.2f},当前仓位{:.2f},当前手数{:.2f}", 
										qty, target_pos, _grid_pos, ctx->stra_get_position(_moncode.c_str())).c_str());
                _grid_pos = target_pos;
			} 
			else 
			{
				ctx->stra_log_info(fmt::format("持仓不做任何变化，当前仓位{:.2f}, 当前手数{:.2f}", _grid_pos,
					ctx->stra_get_position(_moncode.c_str())).c_str());
			}
	}
	kline->release();	// 可以用RAII来自动释放	
}

void WtStraGrid::on_init(ICtaStraCtx* ctx)
{
	std::string code = _code;
	
	_cur_money = 0;
	
	// 缓存日线
	WTSKlineSlice *kline = ctx->stra_get_bars(code.c_str(), "d1", _bar_count, false);
	kline->release();

	// 缓存period线，比如m5
	kline = ctx->stra_get_bars(code.c_str(), _period.c_str(), _bar_count, true);
	ctx->stra_sub_ticks(_code.c_str());
	kline->release();

	auto pInfo = ctx->stra_get_comminfo(code.c_str());
	_vol_scale = pInfo->getVolScale();

	// 初始化价格列表. 价格基准为1
	_price_list.push_back(1);

	// 初始化仓位列表，基准为50%仓位
	_position_list.push_back(0.5);

	// 填充网格
	for (size_t i=0; i < _num_grids; ++i) {
		_price_list.push_back(1 + (i + 1) * _p1);	// 价格上升
		_price_list.push_back(1 + (i + 1) * _p2);	// 价格下降

		_position_list.push_back(0.5 + (i+1) * 0.5 / _num_grids);	// 仓位上升
		_position_list.push_back(0.5 - (i+1) * 0.5 / _num_grids);	// 仓位下降
	}

	// 网格排序
	std::sort(_price_list.begin(), _price_list.end());

	std::sort(_position_list.begin(), _position_list.end());
	std::reverse(_position_list.begin(), _position_list.end());

	std::stringstream ss;
	ss << "grid(price/position): \n";
	for (size_t i=0; i < _price_list.size(); ++i)
	{
		ss << _price_list[i] << "/" << 
			_position_list[i] << "\n";
	}
	
	ctx->stra_log_info(ss.str().c_str());
	//注册指标和图表K线
	ctx->set_chart_kline(_code.c_str(), _period.c_str());
	//注册指标
	ctx->register_index("Grid", 0);
	
	// //注册指标线
	// ctx->register_index_line("Grid", "upper_bound", 0);
	// ctx->register_index_line("DualThrust", "lower_bound", 0);
}

void WtStraGrid::on_tick(ICtaStraCtx* ctx, const char* stdCode, WTSTickData* newTick)
{
	//没有什么要处理
}

void WtStraGrid::on_session_begin(ICtaStraCtx *ctx, uint32_t uTDate)
{
	std::string newMonCode = ctx->stra_get_rawcode(_code.c_str());
	if(newMonCode!=_moncode)
	{
		if(!_moncode.empty())
		{
			double curPos = ctx->stra_get_position(_moncode.c_str());
			if (!decimal::eq(curPos, 0))
			{
				ctx->stra_log_info(fmt::format("主力换月,  老主力{}[{}]将会被清理", _moncode, curPos).c_str());
				ctx->stra_set_position(_moncode.c_str(), 0, "switchout");
				ctx->stra_set_position(newMonCode.c_str(), curPos, "switchin");
			}
		}

		_moncode = newMonCode;
	}
}
