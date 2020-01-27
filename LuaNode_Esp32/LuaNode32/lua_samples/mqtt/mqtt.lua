-- mqtt.new(host, client_id, keepalive, connected_callback, [username], [password], [encrypt])
--   host: the mqtt host IP
--   client_id: client id
--   keepalive: keep alive seconds
--   connected_callback: a callback when connection established
--   username: optional argument, mqtt user name
--   password: 
--   encrypt: not support encrypt now

-- mqtt.on(handle, event, callback): the method is used to register callback to monitor the following event: 'data'/'publish'/'subscribe'/'disconnect'

-- mqtt.unsubscribe(handle, topic): unsubscribe a topic

-- mqtt.close(handle): use this method to close an mqtt connection

-- Note: 
--   1. the mqtt.new retern a handle, which is used for subscribe/publish, remember to save the handle.
--   2. you can create 3 mqtt connections at most. You will fail to create more than 3 mqtt connections.

ccb=function() print("mqtt connected\n"); end
dcb=function(t,d) print(t);print(d); end
wifi.setmode(1);
wifi.start();
wifi.sta.config({ssid="Doit",pwd="doit3305"});
tmr.delay(15);
handle=mqtt.new('test.mosquitto.org','c1',600,ccb);  -- if you don't need connect callback, write like this: mqtt.new('test.mosquitto.org','c1',600,nil)
tmr.delay(8);
mqtt.subscribe(handle,'/chann1',mqtt.QOS0);
tmr.delay(3);
mqtt.on(handle,'data',dcb);
mqtt.publish(handle,'/chann1','test');