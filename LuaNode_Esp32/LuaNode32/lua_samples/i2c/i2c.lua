-- Since there are two i2c on esp32, I setup one i2c as master and the other one as slave, and read/write them on the esp32
-- slave :
-- GPIO25 is assigned as the data signal of i2c slave port
-- GPIO26 is assigned as the clock signal of i2c slave port
-- master:
-- GPIO18 is assigned as the data signal of i2c master port
-- GPIO19 is assigned as the clock signal of i2c master port
-- connect GPIO18 with GPIO25
-- connect GPIO19 with GPIO26 
i2c.setup(i2c.SLAVE,i2c.I2C_0,26,25,40)
i2c.setup(i2c.MASTER,i2c.I2C_1,19,18)
-- slave write something, master read
i2c.write(i2c.SLAVE,i2c.I2C_0,40,"helloworld")
c=i2c.read(i2c.MASTER,i2c.I2C_1,40,10)
print(c)

tmr.delay(5)
-- master write something, slave read
i2c.write(i2c.MASTER,i2c.I2C_1,40,"test")
c=i2c.read(i2c.SLAVE,i2c.I2C_0,40,4)
print(c)

-- i2c uninstall
i2c.uninstall(i2c.I2C_0)
i2c.uninstall(i2c.I2C_1)
