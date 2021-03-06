function tail(arr)
   return { select(2, unpack(arr)) }
end

function countItems(tbl)
   local count = 0
   for _ in pairs(tbl) do count = count + 1 end
   return count
end

function new(obj, cls)
   setmetatable(obj, {__index = cls})
   return obj
end
