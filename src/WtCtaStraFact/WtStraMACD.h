#pragma once
 
#include <vector>

#include "../Includes/CtaStrategyDefs.h"

class WtStraMACD : public CtaStrategy
{
public:
    WtStraMACD(const char* id);
    virtual ~WtStraMACD();
public:
    virtual const char* getFactName() override;

	virtual const char* getName() override;

	virtual bool init(WTSVariant* cfg) override;

	virtual void on_schedule(ICtaStraCtx* ctx, uint32_t curDate, uint32_t curTime) override;

	virtual void on_init(ICtaStraCtx* ctx) override;

	virtual void on_tick(ICtaStraCtx* ctx, const char* stdCode, WTSTickData* newTick) override;

	virtual void on_session_begin(ICtaStraCtx* ctx, uint32_t uTDate) override;

private:
    // 指标参数

    // k线周期
    std::string _period;

    // 一次获取k线条数
    u_int32_t _bar_cnt;

    std::string _code;

    // 保证金率
    double _margin_rate;

    // 每次使用的资金比率
    double _money_pct;

    // 限制每天开仓的次数
    uint32_t _today_entry;

    // 起始资金
    double _capital;

    // 短EMA天数
    uint32_t _k1;

    // 长EMA天数
    uint32_t _k2;
    
    uint32_t _days;

    // 纯私有
    uint32_t _vol_scale;
    std::string _moncode;
};