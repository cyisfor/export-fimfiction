include coolmake/main.mk
coolmake/main.mk: | coolmake
	@echo Coolmake...
	[[ -e $@ ]] || exit 3
	$(S)$(MAKE) $(MAKECMDGOALS)
.PRECIOUS: coolmake/main.mk

coolmake: html_when/coolmake
	ln -rs $< $@

html_when/coolmake: | html_when
	$(MAKE) -C $| $(MAKECMDGOALS)

html_when:
	sh setup.sh
