EXTRAINC = -I../../aux -I../../aux/glad
LDFLAGS = -lglfw
RM ?= rm

disp: main.c constellart.c collage.c glad.o stb.o
	$(CC) -o $@ $^ $(CFLAGS) $(EXTRAINC) $(LDFLAGS)
glad.o: ../../aux/glad/glad.c
	$(CC) -c -o $@ $^ $(CFLAGS) $(EXTRAINC)
stb.o: stb.c
	$(CC) -c -o $@ $^ $(CFLAGS) $(EXTRAINC)
clean:
	$(RM) disp glad.o stb.o

.PHONY: clean
