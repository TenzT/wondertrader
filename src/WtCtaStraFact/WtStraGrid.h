#pragma once

#include <vector>

#include <vector>
#include "../Includes/CtaStrategyDefs.h"

class WtStraGrid : public CtaStrategy
{
public:
	WtStraGrid(const char* id);
	virtual ~WtStraGrid();

public:
	virtual const char* getFactName() override;

	virtual const char* getName() override;

	virtual bool init(WTSVariant* cfg) override;

	virtual void on_schedule(ICtaStraCtx* ctx, uint32_t curDate, uint32_t curTime) override;

	virtual void on_init(ICtaStraCtx* ctx) override;

	virtual void on_tick(ICtaStraCtx* ctx, const char* stdCode, WTSTickData* newTick) override;

	virtual void on_session_begin(ICtaStraCtx* ctx, uint32_t uTDate) override;

private:
	//指标参数
	double		_p1;			// 价格上涨幅度系数
	double		_p2;			// 价格下降幅度系数
	uint32_t	_short_days;	// 计算短时均线时使用的天数
	uint32_t	_long_days;		// 计算长时均线时使用的天数
	uint32_t	_num_grids;

	std::string _moncode;	// 合约月

	// 交易k线的时间级，如5分钟，1分钟
	std::string _period;
	
	// K线条数
	uint32_t	_bar_count;

	//合约代码
	std::string _code;		

	// 起始资金
	double		_capital;

	// 保证金率
	double		_margin_rate;

	// 止损系数，止损点算法为网格最低个的价格 * stop_loss
	double		_stop_loss;

	// 价格模式 0-多、空都支持 1-只能做多 2-只能做空
	bool		_pricemode;	 

	// 价格列表
	std::vector<double> _price_list;

	// 仓位列表
	std::vector<double> _position_list;

	// 一手代表的数量
	uint32_t _vol_scale;

	// 当前现金
	double _cur_money;

	// 基准价格
	double _benchmark_price{1.0};

	// 当前仓位
	double _grid_pos;
};

