print("Loading script...")

function tail(arr)
   return { select(2, unpack(arr)) }
end

CommandTreeNode = {}

CommandTreeNode.new = function()
   local o = {childSet = {}}
   setmetatable(o, {__index = CommandTreeNode})
   return o
end

CommandTreeNode.addCmd = function(self, path, cmd)
   if #path == 0 then self.cmd = cmd return end   
   local key = path[1][1] .. path[1][2]
   
   if not self.childSet[key] then
      self.childSet[key] = CommandTreeNode.new()
   end

   (self.childSet[key]):addCmd(tail(path), cmd)
end

CommandTreeNode.isLeaf = function(self)
   local count = 0
   for _ in pairs(self.childSet) do count = count + 1 end
   return count == 0
end

CommandTreeNode.printPaths = function(self, accum)
   if self:isLeaf() then print(accum) return end
   for k,v in pairs(self.childSet) do
      v:printPaths(accum .. " " .. k)
   end
end

CommandTreeNode.walk = function(self, ch)
   local child = self.childSet[ch[1] .. ch[2]]
   
   if child then
      if child:isLeaf() then
	 return false, child.cmd
      else
	 return true, function (ch) return child:walk(ch) end
      end
   else
      return false, nil
   end
end

keyPressed = {}

wm.grabKey(64, "q")
wm.grabKey(64, "t")
wm.grabKey(64, "g")

wm.onKeyDown(function (win, mods, key)
      keyPressed[key] = true;

      print(key .. mods .. " down")
      
      if key == "q" then
	 wm.quit()
      end

      if key == "t" then
	 print("Grabbing keyboard...")
	 wm.grabKeyboard()
      end

      if key == "g" then
	 print("Releasing keyboard...")
	 wm.releaseKeyboard()
      end
end)

wm.onKeyUp(function (win, mods, key)
      keyPressed[key] = nil;
      print(key .. mods .. " up")
end)

print("Success!")
