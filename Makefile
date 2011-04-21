
PREFIX?=/usr

PROJECT?=TurionPowerControl
PROJ_CXXFLAGS=-O2 $(CXXFLAGS)
PROJ_LDFLAGS=$(LDFLAGS)

SOURCES=TurionPowerControl.cpp \
	config.cpp \
	cpuPrimitives.cpp \
	Griffin.cpp \
	K10Processor.cpp \
	MSRObject.cpp \
	MSVC_Round.cpp \
	PCIRegObject.cpp \
	Processor.cpp \
	scaler.cpp

HEADERS=config.h \
	cpuPrimitives.h \
	Griffin.h \
	K10Processor.h \
	MSRObject.h \
	MSVC_Round.h \
	OlsApi.h \
	OlsDef.h \
	PCIRegObject.h \
	Processor.h \
	scaler.h \
	TurionPowerControl.h

OBJECTS=$(SOURCES:.cpp=.o)

all: $(PROJECT)

i386:
	$(MAKE) CXXFLAGS="-m32 -D_FILE_OFFSET_BITS=64" LDFLAGS="-m32" PROJECT=TurionPowerControl.i386

install: $(PROJECT)
	install -ps $(PROJECT) $(PREFIX)/bin

uninstall:
	$(RM) $(PREFIX)/bin/$(PROJECT)

$(PROJECT): .depend $(OBJECTS)
	$(CXX) $(PROJ_LDFLAGS) -o $@ $(OBJECTS)

dep: .depend

.depend: $(SOURCES) $(HEADERS)
	$(CXX) -MM $(PROJ_CXXFLAGS) -o .depend $(SOURCES)

%.o: %.cpp
	$(CXX) $(PROJ_CXXFLAGS) -c -o $@ $<

clean:
	$(RM) $(OBJECTS) $(PROJECT)
	
distclean: clean
	$(RM) *.o core core.[0-9]
	$(RM) *~ DEADJOE *.orig *.rej *.i *.r[0-9]*
	$(RM) .depend
	$(RM) TurionPowerControl TurionPowerControl.i386

.PHONY: clean distclean dep all install uninstall i386
