-- file create and write
-- To read a file, you should open it first 

res = file.open("myfile.lua","w")  -- Open file myfile.lua. If it doesn't exist, create it, otherwise, overwrite the file
while true do
  if(res == false) then
	print("open file failed");
    break;
  end

  file.write("hello");
  file.close();

  break;
end

--[[
file.open("myfile.lua","w+")  -- Open and append content to myfile.lua
file.write("hello");
file.close();
]]
