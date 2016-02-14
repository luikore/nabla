default: sb/libsb.a

sb/libsb.a: adt/libadt.a
	cd sb; $(MAKE)

adt/libadt.a: vendor/ccut/lib/libccut.a vendor/siphash/libsiphash.a
	cd adt; $(MAKE)

adt/libadt-debug.a: vendor/ccut/lib/libccut.a vendor/siphash/libsiphash.a
	cd adt; $(MAKE) debug

vendor/ccut/lib/libccut.a:
	cd vendor/ccut; $(MAKE)

vendor/siphash/libsiphash.a:
	cd vendor/siphash; $(MAKE)

test: adt/libadt-debug.a
	cd adt; $(MAKE) test
	cd sb; $(MAKE) test

clean:
	cd adt; $(MAKE) clean
	cd sb; $(MAKE) clean

stat:
	@# NOTE bash/zsh glob gives error when no match
	@echo code + tests:
	@cat `ruby -e 'puts Dir.glob "{adt,sb}/**/*.{c,h,rb,S}"'` | wc -l
	@echo tests:
	@cat `ruby -e 'puts Dir.glob "{adt,sb}/**/*test.c"'` | wc -l
	@echo doc:
	@cat `ruby -e 'puts Dir.glob "doc/**/*.md"'` | wc -l
	@echo config:
	@cat `ruby -e 'puts Dir.glob "{adt,sb}/**/{makefile,char-groups}"'` | wc -l
