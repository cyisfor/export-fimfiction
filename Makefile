include coolmake/main.mk
coolmake/main.mk: | coolmake
	$(MAKE)
coolmake: html_when/coolmake
	ln -rs $< $@

html_when/coolmake: | html_when
	$(MAKE) -C $|

html_when:
	sh setup.sh
