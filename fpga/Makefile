CC=lcc-x32
CFLAGS=-I$(X32INC) -I../include
OBJS=fpga.o

.PHONY: clean stubbed stubbedrun uploadrun test

main.ce: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -lib $(X32INC) -o fpga.ce

stubbed: CFLAGS+=-DSENSOR_STUB
stubbed: main.ce

stubbedrun: stubbed uploadrun

init:
	sudo chmod 777 /dev/ttyUSB0

init2:
	sudo chmod 777 /dev/ttyUSB1

clean:
	rm -f fpga.ce $(OBJS) $(OBJS:.o=.d)

uploadrun: main.ce
	x32-upload fpga.ce -c $(X32_PACKAGE_SERIAL) -e

test: test.ce
	x32-upload test.ce -c $(X32_PACKAGE_SERIAL)

test.ce: test.c
	$(CC) $(CFLAGS) test.c -lib $(X32INC) -o test.ce

%.o : %.c
	@$(COMPILE.c) -M -o $@ $< > $*.d
	$(COMPILE.c) -o $@ $<
	@cp $*.d $*.P; \
		sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
			-e '/^$$/ d' -e 's/$$/ :/' < $*.P >> $*.d; \
		rm -f $*.P

-include $(OBJS:.o=.d)
