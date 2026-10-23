-- SPDX-License-Identifier: MIT
--
-- Path of Upgrading's bridge to Path of Building. It runs Path of Building
-- for Path of Exile 2 without its window, so items can be judged by its own
-- calculations for your build.
--
-- The app runs it with LuaJIT in Path of Building's folder, the one with
-- HeadlessWrapper.lua, and sends requests on stdin, a line of JSON each:
--
--   {"id":1,"cmd":"load","xml":"<PathOfBuilding2>..."}
--       Use this build from now on. The answer has its main skill and its
--       stats.
--   {"id":2,"cmd":"equip","items":[{"slot":"Helmet","text":"Item Class: Helmets..."}]}
--       Put these items in the build in place of its own, by Path of
--       Building's names for the slots. The answer is the same as load's,
--       with whether each item went in.
--   {"id":3,"cmd":"eval","items":[{"text":"Item Class: Rings..."}]}
--       The build's numbers with each item in each slot it can go in, the
--       way Path of Building's item tooltips compare them. An item that
--       names a slot is only tried there. The answer has the stats before,
--       and each item's slots with the stats and what it would replace.
--
-- Each answer is a line of JSON after "@@" with the request's id, and ok
-- true, or false with an error in words. The first one comes before any
-- request and says whether Path of Building started. Whatever Path of
-- Building prints goes to stderr. It stops when stdin is closed.

local stdout, stderr = io.stdout, io.stderr

-- Path of Building's own libraries are in lua/ in what rusty-path-of-building
-- downloads, and in runtime/lua/ in a copy of its repository.
package.path = "lua/?.lua;lua/?/init.lua;../runtime/lua/?.lua;../runtime/lua/?/init.lua;" .. package.path
local json = require("dkjson")

-- The Flatpak's LuaJIT has lua-utf8, but LuaJIT from your distribution may
-- not. Path of Building uses it to put separators in numbers and in its text
-- boxes, and string's functions do the same with its usual separators.
if not pcall(require, "lua-utf8") then
    package.loaded["lua-utf8"] = {
        find = string.find,
        gsub = string.gsub,
        match = string.match,
        reverse = string.reverse,
        sub = string.sub,
    }
end

-- stdout is for the answers
function print(...)
    local parts = {}
    for i = 1, select("#", ...) do
        parts[i] = tostring((select(i, ...)))
    end
    stderr:write(table.concat(parts, "\t"), "\n")
end

local function answer(t)
    stdout:write("@@", json.encode(t), "\n")
    stdout:flush()
end

-- Path of Building's text without its colours
local function plain(text)
    return (text:gsub("%^x%x%x%x%x%x%x", ""):gsub("%^%d", ""))
end

-- Path of Building shows errors in a popup, with what to press to get rid of
-- it. Returns the error and gets rid of it, or nil when there's none.
local function promptError()
    local text = launch and launch.promptMsg
    if not text then
        return nil
    end
    launch.promptMsg = nil
    text = plain(text)
    return text:match("^Error:\n\n(.-)\n[^\n]*\nPress Enter/Escape") or text
end

-- When it can't start, Path of Building shows why and waits for Enter.
local read = io.read
io.read = function() end
local started, failure = pcall(dofile, "HeadlessWrapper.lua")
io.read = read
if not started or not build then
    local why = promptError() or failure
    if why then
        answer({ready = false, error = "Path of Building didn't start: " .. tostring(why)})
    else
        answer({ready = false, error = "Path of Building didn't start."})
    end
    os.exit(1)
end
answer({ready = true, version = launch.versionNumber})

-- The numbers items are judged by, by Path of Building's names for them.
local statNames = {
    "FullDPS", "CombinedDPS", "TotalDPS", "TotalEHP",
    "Life", "EnergyShield", "Mana", "Spirit", "SpiritUnreserved",
    "Str", "Dex", "Int", "ReqStr", "ReqDex", "ReqInt",
}
for _, element in ipairs({"Fire", "Cold", "Lightning", "Chaos"}) do
    table.insert(statNames, element .. "Resist")
    table.insert(statNames, element .. "ResistTotal")
    table.insert(statNames, "Missing" .. element .. "Resist")
end

-- JSON has no NaN or infinity.
local function number(value)
    value = tonumber(value)
    if value and value == value and value ~= math.huge and value ~= -math.huge then
        return value
    end
end

-- The minions' damage and life are MinionDPS and MinionLife, and Chaos
-- Inoculation is 1 or 0.
local function stats(output)
    local r = {}
    for _, name in ipairs(statNames) do
        r[name] = number(output[name])
    end
    if output.Minion then
        r.MinionDPS = number(output.Minion.CombinedDPS) or number(output.Minion.TotalDPS)
        r.MinionLife = number(output.Minion.Life)
    end
    r.ChaosInoculation = output.ChaosInoculation and 1 or 0
    return r
end

local unreadable = "Path of Building couldn't read this item."

local function readItem(text)
    local ok, item = pcall(new, "Item", text)
    if ok and item.base then
        return item
    end
end

-- Why there's no build to judge items with, nil when there is one.
local noBuild = "There's no build loaded."

-- What the app shows of the build.
local function summary()
    local group = build.skillsTab.socketGroupList[build.mainSocketGroup]
    return {ok = true, mainSkill = group and group.displayLabel, stats = stats(build.calcsTab.mainOutput)}
end

-- Path of Building asks about some builds in a popup instead of loading them.
-- Returns what it asks and closes it, or nil when there's none.
local function popupError()
    local popup = main.popups[1]
    if not popup then
        return nil
    end
    main.popups = {}
    if popup.title == "Game Version" then
        return "This build is for a version of the game Path of Building doesn't support. "
            .. "Open it in Path of Building to convert it."
    end
    local lines = {}
    for _, control in ipairs(popup.controls) do
        if type(control.label) == "string" then
            table.insert(lines, plain(control.label))
        end
    end
    return #lines > 0 and table.concat(lines, "\n") or popup.title
end

local function load(xml)
    noBuild = "Path of Building couldn't load the build."
    loadBuildFromXML(xml, "")
    local asked = popupError()
    if asked or launch.main.mode ~= "BUILD" or launch.promptMsg then
        noBuild = promptError() or asked or noBuild
        return {ok = false, error = noBuild}
    end
    noBuild = nil
    return summary()
end

local function equip(items)
    local itemsTab = build.itemsTab
    local equipped, results = {}, {}
    for i, entry in ipairs(items) do
        local slot = itemsTab.slots[entry.slot]
        local item = readItem(entry.text)
        if not slot then
            results[i] = {ok = false, error = ("Path of Building has no slot called %s."):format(tostring(entry.slot))}
        elseif not item then
            results[i] = {ok = false, error = unreadable}
        else
            itemsTab:AddItem(item, true)
            slot:SetSelItemId(item.id)
            equipped[i] = {slot = slot, item = item}
        end
    end
    -- which takes items out of slots they can't go in
    itemsTab:PopulateSlots()
    for i, e in pairs(equipped) do
        if e.slot.selItemId == e.item.id then
            results[i] = {ok = true}
        else
            results[i] = {ok = false, error = ("This item can't go in %s."):format(e.slot.slotName)}
        end
    end
    build.buildFlag = true
    runCallback("OnFrame")
    local r = summary()
    r.results = results
    return r
end

-- The slots an item is compared in when it doesn't name one, the way Path of
-- Building's item tooltips pick them: those it can go in that are in use, in
-- the weapon set you use.
local function slotsFor(item)
    local itemsTab = build.itemsTab
    local weaponSet = itemsTab.activeItemSet.useSecondWeaponSet and 2 or 1
    local slots = {}
    for _, slot in ipairs(itemsTab.orderedSlots) do
        if itemsTab:IsItemValidForSlot(item, slot.slotName) and not slot.inactive
            and (not slot.weaponSet or slot.weaponSet == weaponSet) and slot.shown() then
            table.insert(slots, slot)
        end
    end
    return slots
end

local function evaluate(entry, calculate)
    local item = readItem(entry.text)
    if not item then
        return {ok = false, error = unreadable}
    end
    local slots
    if entry.slot then
        slots = {build.itemsTab.slots[entry.slot]}
        if not slots[1] then
            return {ok = false, error = ("Path of Building has no slot called %s."):format(tostring(entry.slot))}
        end
        if not build.itemsTab:IsItemValidForSlot(item, entry.slot) then
            return {ok = false, error = ("This item can't go in %s."):format(entry.slot)}
        end
    else
        slots = slotsFor(item)
    end
    if #slots == 0 then
        if item.type == "Jewel" then
            return {ok = false, error = "None of the build's jewel sockets can take this jewel."}
        end
        return {ok = false, error = "There's nowhere in the build this item can go."}
    end
    local results = {}
    for _, slot in ipairs(slots) do
        local current = build.itemsTab.items[slot.selItemId]
        table.insert(results, {
            slot = slot.slotName,
            label = slot.label,
            replacing = current and current.name,
            stats = stats(calculate({repSlotName = slot.slotName, repItem = item})),
        })
    end
    return {ok = true, slots = results}
end

local function eval(items)
    -- which also numbers the jewel sockets
    build.itemsTab:UpdateSockets()
    local calculate, output = build.calcsTab:GetMiscCalculator()
    local results = {}
    for i, entry in ipairs(items) do
        local ok, r = pcall(evaluate, entry, calculate)
        results[i] = ok and r or {ok = false, error = tostring(r)}
    end
    return {ok = true, before = stats(output), results = results}
end

local commands = {
    load = function(request)
        return load(request.xml)
    end,
    equip = function(request)
        if noBuild then
            return {ok = false, error = noBuild}
        end
        return equip(request.items or {})
    end,
    eval = function(request)
        if noBuild then
            return {ok = false, error = noBuild}
        end
        return eval(request.items or {})
    end,
}

for line in io.lines() do
    local request = json.decode(line)
    local r
    if type(request) ~= "table" or type(request.cmd) ~= "string" then
        r = {ok = false, error = "That isn't a request."}
    elseif not commands[request.cmd] then
        r = {ok = false, error = ("There's no command called %s."):format(request.cmd)}
    else
        launch.promptMsg = nil
        local ok, result = pcall(commands[request.cmd], request)
        local shown = promptError()
        if not ok then
            r = {ok = false, error = tostring(result)}
        elseif shown and result.ok then
            r = {ok = false, error = shown}
        else
            r = result
        end
    end
    if type(request) == "table" then
        r.id = request.id
    end
    answer(r)
end
