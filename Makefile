all:
    gcc server/battleserver.c -o server/battleserver -pthread
    gcc client/battleclient.c -o client/battleclient

clean:
    rm -f server/battleserver client/battleclient
