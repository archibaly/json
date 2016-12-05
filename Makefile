EXE = json
CFLAGS = -Wall
LDFLAGS = -lm

$(EXE): main.o json.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *.o $(EXE)
