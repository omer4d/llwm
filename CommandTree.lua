-- ********
-- * Node *
-- ********

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

-- *************
-- * Traverser *
-- *************

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
