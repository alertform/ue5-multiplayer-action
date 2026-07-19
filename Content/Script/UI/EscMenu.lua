--[[
  ESC 菜单（UI.EscMenu）—— 菜单项与点击行为全在本文件。
  C++（UMAEscMenuWidget）只提供面板骨架与 AddButton 构件 API；
  想加/删/改菜单项，改这里保存即可，零重编。
]]

local M = UnLua.Class()

function M:OnMenuOpened()
    if self.bBuilt then return end
    self.bBuilt = true

    local pc = self:GetOwningPlayer()

    local resume = self:AddButton("继 续")
    resume.OnClicked:Add(self, function() pc:CloseEscMenu() end)

    local journal = self:AddButton("任务日志")
    journal.OnClicked:Add(self, function()
        pc:CloseEscMenu()
        pc:ToggleQuestJournal()
    end)

    local mainmenu = self:AddButton("返回主菜单")
    mainmenu.OnClicked:Add(self, function() pc:ReturnToMainMenu() end)

    local quit = self:AddButton("退出游戏")
    quit.OnClicked:Add(self, function() pc:QuitToDesktop() end)
end

return M
