target=test
CCFLAG= -g -pthread
objs=$(addsuffix .o, $(basename $(wildcard *.c *.cpp)))
all:$(target)
$(target) : $(objs)
	g++ $(CCFLAG) -o $@ $<

%.o:%.c %.h
	gcc $(CCFLAG) -c  $< -o $@

%.o:%.cpp %.h
	g++ $(CCFLAG) -c  $< -o $@

clean:
	rm *.o $(target) -f
