TEX := xelatex
MAIN := main

.PHONY: all clean check

all:
	$(TEX) -interaction=nonstopmode -halt-on-error $(MAIN).tex
	$(TEX) -interaction=nonstopmode -halt-on-error $(MAIN).tex
	$(TEX) -interaction=nonstopmode -halt-on-error $(MAIN).tex

check:
	@! grep -E '(LaTeX Warning: Reference.*undefined|There were undefined references|Overfull \\hbox)' $(MAIN).log

clean:
	rm -f $(MAIN).aux $(MAIN).bbl $(MAIN).bcf $(MAIN).blg $(MAIN).lof \
	      $(MAIN).log $(MAIN).lot $(MAIN).out $(MAIN).run.xml $(MAIN).toc
