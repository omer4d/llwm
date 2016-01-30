print("Loading script...")

function tail(arr)
   return { select(2, unpack(arr)) }
end

function countItems(tbl)
   local count = 0
   for _ in pairs(tbl) do count = count + 1 end
   return count
end

CommandTreeNode = {}

CommandTreeNode.new = function()
   local o = {childSet = {}}
   setmetatable(o, {__index = CommandTreeNode})
   return o
end

CommandTreeNode.findChild = function(self, ch)
   for k,v in pairs(self.childSet) do
      if k[1] == ch[1] and k[2] == ch[2] then return v end
   end
   return nil
end

CommandTreeNode.addCmd = function(self, path, cmd)
   if #path == 0 then self.cmd = cmd return end   
   local child = self:findChild(path[1])
   
   if not child then
      child = CommandTreeNode.new()
      self.childSet[path[1]] = child
   end
   
   child:addCmd(tail(path), cmd)
end

CommandTreeNode.isLeaf = function(self)
   return countItems(self.childSet) == 0
end

CommandTreeNode.printPaths = function(self, accum)
   if self:isLeaf() then print(accum) return end
   for k,_ in pairs(self.childSet) do
      v:printPaths(accum .. " " .. k[2] .. "(" .. k[1] .. ")")
   end
end

CommandTreeTraverser = {}

CommandTreeTraverser.new = function(root, callback)
   local o = {root = root, callback = callback, currNode = root}
   setmetatable(o, {__index = CommandTreeTraverser})
   return o
end

CommandTreeTraverser.step = function(self, ch)
   local child = self.currNode:findChild(ch)
   
   if child then
      if child:isLeaf() then
	 self.currNode = self.root
	 self.callback(child.cmd)
	 return false
      else
	 self.currNode = child
	 return true
      end
   else
      self.currNode = self.root
      self.callback(nil)
      return false
   end
end

function grabRootKeys(root)
   for k,_ in pairs(root.childSet) do
      wm.grabKey(k[1], k[2])
   end
end

function cmdTreeTravCb(cmd)
   if cmd then cmd() else print("Unknown command sequence!") end
end

MOD_KEYS = {
   Super_L = true,
   Super_R = true,
   Control_L = true,
   Control_R = true,
   Shift_L = true,
   Shift_R = true,
   Alt_L = true,
   Alt_R = true
}

cmdTree = CommandTreeNode.new()

cmdTree:addCmd({{64, "q"}}, function()
      print("Quitting...")
      wm.quit()
end)

cmdTree:addCmd({{64, "t"}}, function()
      os.execute("urxvt &")
end)

cmdTree:addCmd({{64, "x"}, {0, "r"}}, function()
      print("Reloading!")
      dofile("default.lua")
end)

cmdTraverser = CommandTreeTraverser.new(cmdTree, cmdTreeTravCb)

keyPressed = {}
scheduledKbRelease = false
wm.releaseAllKeys()
grabRootKeys(cmdTree)

wm.onKeyDown(function (win, mods, key)
      if not keyPressed[key] and not MOD_KEYS[key] then
	 --print(key .. mods .. " pressed")
	 
	 if cmdTraverser:step({mods, key}) then wm.grabKeyboard() else scheduledKbRelease = true end
      end
      
      keyPressed[key] = true;
end)

wm.onKeyUp(function (win, mods, key)
      --print(key .. " released")
      keyPressed[key] = nil;

      if countItems(keyPressed) == 0 and scheduledKbRelease then
	 wm.releaseKeyboard()
	 scheduledKbRelease = false
      end
end)

wm.onWindowCreated(function (win)
      print("Created window " .. win)
      wm.setWindowFrame(win, 0, 0, 500, 500)
end)

wm.onWindowDestroyed(function (win)
      print("Destroyed window " .. win)
end)

print("Success!")
