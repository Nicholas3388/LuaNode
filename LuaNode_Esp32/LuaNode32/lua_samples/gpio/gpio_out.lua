-- GPIO sample
-- The pin gpio2 connect to a led. when the pin output high level, the led will be light up
-- gpio.mode(pin, mode)
-- mode = 1(INPUT), 2(OUTPUT),4(PULLUP),5(INPUT_PULLUP),8(PULLDOWN),9(INPUT_PULLDOWN)......
-- gpio.write(pin, level)
-- level = gpio.read(pin)

gpio.mode(2,gpio.OUTPUT);
gpio.write(2,1);  -- led on
gpio.write(2,0);  -- led off

