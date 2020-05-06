FLAGS=-Wall -Wextra -lm -libverbs -lpthread -D _GNU_SOURCE -std=gnu99 -I.
CC=gcc
EXECS = http_server server test
ODIR=ofiles

_HTTP_SERVER_OBJ = nweb.o kv_client.o establishment.o memoryCache.o dkv_client.o helper.o
_SERVER_OBJ = server.o establishment.o memoryPool.o
_TEST_OBJ = dkv_client.o test.o kv_client.o establishment.o memoryCache.o helper.o

HTTP_SERVER_OBJ = $(patsubst %,$(ODIR)/%,$(_HTTP_SERVER_OBJ))
SERVER_OBJ = $(patsubst %,$(ODIR)/%,$(_SERVER_OBJ))
TEST_OBJ = $(patsubst %,$(ODIR)/%,$(_TEST_OBJ))

all: $(EXECS)

# --------------- .O files creation ---------------:

$(ODIR)/nweb.o: nweb.c nweb.h dkv_client.h helper.h
	$(CC) -o $@ -c $< $(FLAGS)

$(ODIR)/establishment.o: establishment.c establishment.h
	$(CC) -o $@ -c $< $(FLAGS)

$(ODIR)/kv_client.o: kv_client.c kv_client.h establishment.h
	$(CC) -o $@ -c $< $(FLAGS)

$(ODIR)/dkv_client.o: dkv_client.c dkv_client.h kv_client.c kv_client.h helper.h
	$(CC) -o $@ -c $< $(FLAGS)

$(ODIR)/server.o: server.c server.h establishment.h
	$(CC) -o $@ -c $< $(FLAGS)

$(ODIR)/memoryPool.o: memoryPool.c memoryPool.h
	$(CC) -o $@ -c $< $(FLAGS)

$(ODIR)/memoryCache.o: memoryCache.c memoryCache.h
	$(CC) -o $@ -c $< $(FLAGS)

$(ODIR)/helper.o: helper.c helper.h
	$(CC) -o $@ -c $< $(FLAGS)

$(ODIR)/test.o: test.c dkv_client.h
	$(CC) -o $@ -c $< $(FLAGS)

# --------------- executables creations ---------------:

http_server: $(HTTP_SERVER_OBJ)
	$(CC) -o $@ $^ $(FLAGS)

server: $(SERVER_OBJ)
	$(CC) -o $@ $^ $(FLAGS)

test: $(TEST_OBJ)
	$(CC) -o $@ $^ $(FLAGS)

# --------------- valgrind commands ---------------:

val_server:
	valgrind --leak-check=full --track-origins=yes server

val_test:
	valgrind --leak-check=full --track-origins=yes test

.PHONY: clean

clean: 
	rm -f $(ODIR)/*.o $(EXECS)
