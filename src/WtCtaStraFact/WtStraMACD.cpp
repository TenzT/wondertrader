#include "WtStraMACD.h"

#include "../Includes/ICtaStraCtx.h"

#include "../Includes/WTSContractInfo.hpp"
#include "../Includes/WTSVariant.hpp"
#include "../Includes/WTSDataDef.hpp"
#include "../Share/decimal.h"
#include <cmath>

extern const char* FACT_NAME;

//By TenzT @2024.11.17
#include "../Share/fmtlib.h"

WtStraMACD::WtStraMACD(char const* id)
    : CtaStrategy(id)
{
}

WtStraMACD::~WtStraMACD()
{
}

const char* WtStraMACD::getFactName()
{
	return FACT_NAME;
}

const char* WtStraMACD::getName() {
	return "MACD";
}

bool WtStraMACD::init(WTSVariant* cfg) {
    if (cfg == NULL) {
        return false;
    }

    _period = cfg->getCString("period");
    _bar_cnt = cfg->getUInt32("bar_count");
    _code = cfg->getCString("code");
    _margin_rate = cfg->getDouble("margin_rate");
    _money_pct = cfg->getDouble("money_pct");
    _today_entry = cfg->getUInt32("today_entry");
    _capital = cfg->getDouble("capital");
    _k1 = cfg->getUInt32("k1");
    _k2 = cfg->getUInt32("k2");
    
    return true;
}

void WtStraMACD::on_schedule(ICtaStraCtx *ctx, uint32_t curDate, uint32_t curTime)
{
    double cur_pos = ctx->stra_get_position(_moncode.c_str());
    if (cur_pos == 0) {
        // 初始资金 + 当前盈亏计算出可用的钱
        _cur_money = _capital + ctx->stra_get_fund_data(0);
    }

    // 一手所需要的钱
    double cur_price = ctx->stra_get_price(_code.c_str());
    double trdUnit_price = _vol_scale * _margin_rate * cur_price;
    
    WTSKlineSlice *kline = ctx->stra_get_bars(_code.c_str(), "d1", _bar_cnt, true);
    // TODO: 实现ema
}

void WtStraMACD::on_init(ICtaStraCtx* ctx)
{
    // 缓存日线
    WTSKlineSlice *kline = ctx->stra_get_bars(_code.c_str(), "d1", _bar_cnt, true);
    kline->release();

	auto pInfo = ctx->stra_get_comminfo(_code.c_str());
	_vol_scale = pInfo->getVolScale();

    _cur_money = 0;

	//注册指标和图表K线
	ctx->set_chart_kline(_code.c_str(), "d1");
	//注册指标
	ctx->register_index("MACD", 0);
}

void WtStraMACD::on_tick(ICtaStraCtx* ctx, const char* stdCode, WTSTickData* newTick)
{
	//没有什么要处理
}

void WtStraMACD::on_session_begin(ICtaStraCtx *ctx, uint32_t uTDate)
{
	std::string newMonCode = ctx->stra_get_rawcode(_code.c_str());
	if(newMonCode!= _moncode)
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
