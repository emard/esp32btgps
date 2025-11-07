all: plot

PLOT=roadrun-frf.interactive

SPICE=rlwrap ngspice
NUTMEG=rlwrap ngnutmeg

#SPICE=rlwrap /opt/ngspice25/bin/ngspice
#NUTMEG=rlwrap /opt/ngspice25/bin/ngnutmeg

MODELS=qcar_hsri.spice qcar_bpr.spice

FRF_AXLE_FILE=frf_road_axle_car
FRF_BODY_FILE=frf_road_body_car

qcar_%.spice: params-%.maxima
	maxima --batch=$<

# generate model and plot its characteristics
#plot: $(TARGET) $(PLOT)
plot: $(PLOT)

models: $(MODELS)

#$(TARGET): inputdata-bimbo.maxima coil-spice-model-04.maxima
#	maxima --batch=inputdata-bimbo.maxima

%.interactive: %.spice models
	$(SPICE) $<
	./frf2csv.sh < $(FRF_AXLE_FILE).txt | gzip -9 > $(FRF_AXLE_FILE).csv.gz
	./frf2csv.sh < $(FRF_BODY_FILE).txt | gzip -9 > $(FRF_BODY_FILE).csv.gz

clean:
	rm -f $(TARGET) $(MODELS) iri.ps $(FRF_AXLE_FILE).* $(FRF_BODY_FILE).* *~
