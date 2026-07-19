--[[
  任务日志面板（UI.QuestJournal）—— 界面逻辑全在本文件。
  C++（UMAQuestJournalWidget）只提供骨架容器与 AddLine/ClearLines 构件 API；
  本模块决定显示什么、怎么排、多久刷 —— 改完保存，重开面板（J 两次）即生效。
  数据全部来自复制到本端的 PlayerState 只读访问器：lua 是纯表现层，权威在服务器。
]]

local M = UnLua.Class()

local COL_TEXT   = UE.FLinearColor(0.92, 0.92, 0.92, 1)
local COL_ACCENT = UE.FLinearColor(1.0, 0.78, 0.35, 1)
local COL_TRUST  = UE.FLinearColor(0.55, 0.85, 1.0, 1)
local COL_DIM    = UE.FLinearColor(0.6, 0.6, 0.65, 1)

local function GetPS(self)
    local pc = self:GetOwningPlayer()
    return pc and pc.PlayerState or nil
end

-- 面板打开：重建行结构（持引用），随后靠 OnJournalRefresh 只改文本。
function M:OnJournalOpened()
    self:ClearLines()
    self.QuestLine  = self:AddLine(15, COL_TEXT)
    self.TimerLine  = self:AddLine(14, COL_ACCENT)
    self.BuffLine   = self:AddLine(14, COL_ACCENT)
    self.FavorLine  = self:AddLine(14, COL_TRUST)
    self.StatsLine  = self:AddLine(13, COL_DIM)
    self.HintLine   = self:AddLine(12, COL_DIM)
    self.FooterLine = self:AddLine(11, COL_DIM)
    self.FooterLine:SetText("按 J 关闭")
    self:OnJournalRefresh()
end

-- 可见期间 C++ 每 0.25s 打一次脉冲：倒计时/进度实时跳字。
function M:OnJournalRefresh()
    if not self.QuestLine then return end
    local ps = GetPS(self)
    if not ps then return end

    local q = ps:GetActiveQuestState()
    if q.Phase == UE.EMAQuestPhase.Active then
        self.QuestLine:SetText(string.format("当前委托：击杀 %d / %d", q.Progress, q.TargetKills))
        if q.DeadlineServerTime > 0 then
            local gs = UE.UGameplayStatics.GetGameState(self)
            local now = gs and gs:GetServerWorldTimeSeconds() or 0
            local remain = math.max(0, math.floor(q.DeadlineServerTime - now))
            self.TimerLine:SetText(string.format("剩余时间 %02d:%02d", math.floor(remain / 60), remain % 60))
        else
            self.TimerLine:SetText("不限时")
        end
    elseif q.Phase == UE.EMAQuestPhase.Completed then
        self.QuestLine:SetText("上一委托已完成，可再接")
        self.TimerLine:SetText("")
    elseif q.Phase == UE.EMAQuestPhase.Failed then
        self.QuestLine:SetText("上一委托超时失败")
        self.TimerLine:SetText("")
    else
        self.QuestLine:SetText("暂无委托 —— 找云游剑客接任务")
        self.TimerLine:SetText("")
    end

    -- 当前增益（C++ 只报 GE_Buff_* 限时增益，冷却等噪音已过滤）
    local buffs = ps:GetActiveBuffLines()
    if buffs and buffs:Length() > 0 then
        local parts = {}
        for i = 1, buffs:Length() do
            parts[#parts + 1] = buffs:Get(i)
        end
        self.BuffLine:SetText("增益：" .. table.concat(parts, " · "))
    else
        self.BuffLine:SetText("增益：无")
    end

    local favor = ps:GetNpcFavor()
    local attitude = "中立"
    if favor >= 3 then attitude = "信任"
    elseif favor <= -3 then attitude = "恼怒"
    elseif favor < 0 then attitude = "不满" end
    self.FavorLine:SetText(string.format("剑客好感 %+d（%s）", favor, attitude))

    self.StatsLine:SetText(string.format("本局战绩  击杀 %d / 阵亡 %d", ps:GetKills(), ps:GetDeaths()))
    self.HintLine:SetText(favor >= 3
        and "已解锁：对话可打探对局情报、可求赐福"
        or  "好感 ≥ +3 解锁：情报打探与全场赐福")
end

return M
