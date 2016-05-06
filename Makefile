DIR=.build
LIB=lib
PNX=./bin/c2h5oh_nginx
NX=$(DIR)/nginx/objs/nginx 
NXO=$(DIR)/nginx/objs/addon/nginx_c2h5oh/*.o
CMAKE=cmake ..

all: $(PNX)

debug: CMAKE=cmake -DCMAKE_BUILD_TYPE=Debug ..
debug: cmake 
debug: compile
debug: $(PNX)

release: CMAKE=cmake -DCMAKE_BUILD_TYPE=Release ..
release: cmake 
release: compile
release: $(PNX)

nginx: nginx_drop all

drop: nginx_drop clean
	rm -rdf ./$(DIR)
	rm -f ./$(LIB)/*

deb:
	$(DIR)/deploy_c2h5oh.sh

test:
	$(MAKE) -C $(DIR) check

nginx_drop:
	rm -rdf $(NXO)
	rm -f $(NX)
	rm -f $(PNX)

$(PNX): compile
	$(MAKE) -C $(DIR)/nginx 
	cp -f $(NX) $(PNX)

compile: 
	$(MAKE) -C $(DIR)

clean:
	- $(MAKE) -C $(DIR) clean
	- $(MAKE) -C $(DIR)/nginx clean
	rm -f ./$(LIB)/*

cmake: $(DIR)
	cd $(DIR) && $(CMAKE)
	
$(DIR):
	mkdir -p ./$(DIR)

