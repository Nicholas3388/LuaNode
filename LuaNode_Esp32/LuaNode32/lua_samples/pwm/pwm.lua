-----------------------------------------------------------------------------------
-- The following methods are provided by PWM module:
--
-- 1. pwm.setup(pin, freqency, duty, channel)
--      pin: pin to set
--      frequency: frequency to set
--      duty: duty to set. range from 0 to 8192
--      channel: used channel, valid channel range from 0 to 7
-- 2. pwm.stop(channel)
-- 3. pwm.getduty(channel)
-- 4. pwm.setduty(channel, duty)
-- 5. pwm.getfreq()
-- 6. pwm.setfreq(frequency)
--
-- Note: once pwm.setup is called, your device start to generate PWM.
-----------------------------------------------------------------------------------

-- The following example is to make the blue LED on DOIT ESP32 dev-board blink per seconds. 
-- There is a blue LED connect to GPIO2 on DOIT ESP32 dev-board.

pwm.setup(2, 1, 4096, 0); -- blink per seconds
duty=pwm.getduty(0);
print(duty);
freq = pwm.getfreq();
print(freq);
tmr.delay(10);
pwm.setfreq(2); -- blink twice per seconds
tmr.delay(10);
pwm.stop(0);