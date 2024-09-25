EXTRAINC = -I../../aux -I../../aux/glad -I../../aux/glfw-3.4/include
LDFLAGS = -L../../aux/glfw-3.4/lib-x86_64 -lglfw3 -framework Cocoa -framework OpenGL -framework IOKit
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
