static inline unsigned long arch_local_irq_save(void)
{
        unsigned long flags;
        __asm__ __volatile__("rsil %0, 1" : "=a" (flags) :: "memory");
        return flags;
}

static inline void arch_local_irq_restore(unsigned long flags)
{
        __asm__ __volatile__("wsr %0, ps; rsync" :: "a" (flags) : "memory");
}

int __atomic_fetch_add_4(int *v, int i, int model)
{
	(void)model;

	unsigned long tmp = arch_local_irq_save();
	int vv = *v;
	*v += i;
	arch_local_irq_restore(tmp);

	return vv;
}
