-- The uart module provide the following method
--
-- 1. uart.setup(id, baud, databits, parity, stopbits, flow_ctrl, [txd], [rxd], [rts], [cts])
--      id: the uart number
--      baud: the uart baud rate
--      databits: can be the following constants: uart.DATA_8_BITS/uart.DATA_7_BITS/uart.DATA_6_BITS/uart.DATA_5_BITS
--      parity: can be uart.PARITY_NONE/uart.PARITY_ODD/uart.PARITY_EVEN
--      stopbits: can be uart.STOPBITS_1/uart.STOPBITS_1_5/uart.STOPBITS_2
--      flow_ctrl: flow control enable. normally set to uart.FLOWCTRL_DISABLE
--      txd: pin of txd, optional, default pin is GPIO4
--      rxd: pin of rxd, optional, default pin is GPIO5
--      rts: pin of rts, optional, default pin is GPIO18
--      cts: pin of cts, optional, default is GPIO19
-- 2. uart.write(id, string1, [string2], ..., [stringn])
-- 3. uart.uninstall(id)

uart.setup(uart.UART_1, 115200, uart.DATA_8_BITS, uart.PARITY_NONE, uart.STOPBITS_1, uart.FLOWCTRL_DISABLE);
while true do
  tmr.delay(2);
  uart.write(1, "hello");
end

-- uart.uninstall(uart.UART_1)