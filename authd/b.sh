cc -g -Wall -Werror -DDEBUG -c -o util.o util.c
cc -g -Wall -Werror -DDEBUG -c -o time.o time.c
cc -g -Wall -Werror -DDEBUG -c -o sd-daemon.o sd-daemon.c
cc -g -Wall -Werror -DDEBUG -c -o sock-bsd.o sock-bsd.c
cc -g -Wall -Werror -DDEBUG -c -o net.o net.c
cc -g -Wall -Werror -DDEBUG -c -o server.o server.c
cc -g -Wall -Werror -DDEBUG -c -o connections.o connections.c
cc -g -Wall -Werror -DDEBUG -c -o protocol.o protocol.c
cc -g -Wall -Werror -DDEBUG -c -o main.o main.c
cc -DDEBUG -o authd *.o
