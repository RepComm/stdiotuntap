
gcc ./src/main.c \
  -ltuntap \
  -g \
  -L./ \
  -Wl,-rpath='${ORIGIN}' \
  -Wall \
  -o stdiotuntap

chmod +x ./stdiotuntap

echo done compiling
