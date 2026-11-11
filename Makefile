# 顶层：逐目录构建
DIRS := ch03/3_1_cache ch03/3_2_vm_alloc ch03/3_x_container

.PHONY: all clean help $(DIRS)

all: $(DIRS)

$(DIRS):
	@$(MAKE) --no-print-directory -C $@ all

clean:
	@for d in $(DIRS); do $(MAKE) --no-print-directory -C $$d clean; done

help:
	@echo "make            构建全部实验"
	@echo "make clean      清理"
	@echo "各实验单独构建：cd ch03/<子目录> && make"
