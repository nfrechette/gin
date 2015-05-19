SCRIPTS=./script

.PHONY:	test
test:
	python $(SCRIPTS)/test.py

.PHONY:	clean
clean:
	python $(SCRIPTS)/clean.py

