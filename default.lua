print("Loading script...")

dofile("Util.lua")
dofile("CommandTree.lua")
dofile("FrameTree.lua")

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


root = Vsplit.new(0.7)
hs = Hsplit.new(0.3)
root:add(hs);

function retile()
   root:update(0, 0, 800, 600)
end

function openWindow(handle)
   local win = Window.new(handle)
   root:add(win)
   retile();
end

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
      openWindow(win)
      --wm.setWindowFrame(win, 0, 0, 500, 500)
end)

wm.onWindowDestroyed(function (win)
      --retile()
      print("Destroyed window " .. win)
end)

print("Success!")
