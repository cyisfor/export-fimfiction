include coolmake/main.mk
coolmake/main.mk: | coolmake
	@echo Coolmake...
	@if [[ -z "$(CLEANING)" ]]; then \
		[[ -e $@ ]] || exit 3; \
		$(MAKE) $(MAKECMDGOALS); \
	fi
.PRECIOUS: coolmake/main.mk

coolmake: html_when/coolmake
	ln -rs $< $@

html_when/coolmake: | html_when
	$(MAKE) -C $| $(MAKECMDGOALS)

html_when:
	sh setup.sh
