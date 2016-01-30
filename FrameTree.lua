-- *****************
-- * FrameTreeNode *
-- *****************

FrameTreeNode = {}

FrameTreeNode.new = function()
   local o = {children = {}}
   setmetatable(o, {__index = FrameTreeNode})
   o:setBounds(0, 0, -1, -1)
   return o
end

FrameTreeNode.containsWindow = function(self)
   for _,child in ipairs(self.children) do
      if child:containsWindow() then return true end
   end
   
   return false
end

FrameTreeNode.accepts = function(self, node)
   return true;
end

FrameTreeNode.add = function(self, node)
   if self:accepts(node) then
      table.insert(self.children, node)
      return true
   else
      for _,child in ipairs(self.children) do
	 if child:add(node) then return true end
      end
      return false;
   end
end

FrameTreeNode.setBounds = function(self, x, y, w, h)
   self.x = x;
   self.y = y;
   self.w = w;
   self.h = h;
end

-- **********
-- * Vsplit *
-- **********

Vsplit = {}
setmetatable(Vsplit, {__index = FrameTreeNode})

Vsplit.new = function(ratio)
   local o = FrameTreeNode.new()
   setmetatable(o, {__index = Vsplit})
   o.ratio = ratio
   return o
end

Vsplit.accepts = function(self, node)
   return countItems(self.children) < 2
end

Vsplit.update = function(self, x, y, w, h)
    local a = self.children[1]
    local b = self.children[2]
    local f1 = a and a:containsWindow()
    local f2 = b and b:containsWindow()
    local h1 = h * ((f1 and f2) and self.ratio or (f1 and 1.0 or 0.0))
    local h2 = h - h1
    
    if f1 then a:update(x, y, w, h1) end
    if f2 then b:update(x, y + h1, w, h2) end

    self:setBounds(x, y, w, h)
end

-- **********
-- * Hsplit *
-- **********

Hsplit = {}
setmetatable(Hsplit, {__index = FrameTreeNode})

Hsplit.new = function(ratio)
   local o = FrameTreeNode.new()
   setmetatable(o, {__index = Hsplit})
   o.ratio = ratio
   return o
end

Hsplit.accepts = function(self, node)
   return countItems(self.children) < 2
end

Hsplit.update = function(self, x, y, w, h)
    local a = self.children[1]
    local b = self.children[2]
    local f1 = a and a:containsWindow()
    local f2 = b and b:containsWindow()
    local w1 = w * ((f1 and f2) and self.ratio or (f1 and 1.0 or 0.0))
    local w2 = w - w1

    if f1 then a:update(x, y, w1, h) end
    if f2 then b:update(x + w1, y, w2, h) end
    
    self:setBounds(x, y, w, h)
end

-- **********
-- * Single *
-- **********

Single = {}
setmetatable(Single, {__index = FrameTreeNode})

Single.new = function()
   local o = FrameTreeNode.new()
   setmetatable(o, {__index = Single})
   return o
end

Single.update = function(self, x, y, w, h)
   for i=#self.children,1,-1 do
      local n = self.children[i]
      if(n:containsWindow()) then
	 n:update(x, y, w, h)
	 break
      end
   end
   
   self:setBounds(x, y, w, h)
end

-- **********
-- * Window *
-- **********

Window = {}

Window.new = function(handle)
   local o = {x = 0, y = 0, w = -1, h = -1, handle = handle}
   setmetatable(o, {__index = Window})
   return o
end

Window.update = function(self, x, y, w, h)
   self.x = x
   self.y = y
   self.w = w
   self.h = h
   wm.setWindowFrame(self.handle, x, y, w, h)
end

Window.containsWindow = function(self)
   return true;
end
