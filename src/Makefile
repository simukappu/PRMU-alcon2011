# compiler
# g++ for C++ code
CC= g++

# options
CFLAGS= -g -O2 `pkg-config --cflags opencv` 
LIBS= -lm `pkg-config --libs opencv`
INCLUDES= 

OBJECTS= alcon2011_main.o user_function.o
TARGET=  alcon2011



# programs
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

# clean
clean:
	rm -f *.[o]
	rm $(TARGET)
	rm *~

# objects
.cc.o:
	$(CC) $(INCLUDES) $(CFLAGS) -c $<
