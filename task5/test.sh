echo "Hello, world!" > /dev/fifo-0
echo "Bye bye!" > /dev/fifo-0
echo "1234" > /dev/fifo-0
cat /dev/fifo-0  # 1234
echo "Hi" > /dev/fifo-0
cat /dev/fifo-0  # Hi
cat /dev/fifo-0  # Bye bye!
cat /dev/fifo-0  # Hello, world!
cat /dev/fifo-0  # nothing
