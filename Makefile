include coolmake/main.mk
coolmake/main.mk: | coolmake
	@echo Coolmake...
	@[[ -n "$(CLEANING)" ]] && exit 0
	@[[ -e $@ ]] || exit 3
	@$(MAKE) $(MAKECMDGOALS)
.PRECIOUS: coolmake/main.mk

coolmake: html_when/coolmake
	ln -rs $< $@

html_when/coolmake: | html_when
	$(MAKE) -C $| $(MAKECMDGOALS)

html_when:
	sh setup.sh
