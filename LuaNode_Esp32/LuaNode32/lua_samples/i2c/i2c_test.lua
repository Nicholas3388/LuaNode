-- ESP32 Lua I2c bus scanner
-- vslinuxdotnet@github - 11/02/2017

sda=18
scl=19

function find_dev(dev_addr)
     c=i2c.read(i2c.MASTER, i2c.I2C_1, dev_addr, 4)
     if c == "" then
        return false
     else
       return true
     end
end

i2c.setup(i2c.MASTER, i2c.I2C_1, scl, sda)
print("Scanning I2C Bus...")
for i=0,127 do
     if find_dev(i)==true then
        print("Device found at address 0x"..string.format("%02X",i).."!")
     end
end
print("Done!")