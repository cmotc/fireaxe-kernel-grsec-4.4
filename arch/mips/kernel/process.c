/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 - 1999, 2000 by Ralf Baechle and others.
 * Copyright (C) 2005, 2006 by Ralf Baechle (ralf@linux-mips.org)
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 * Copyright (C) 2004 Thiemo Seufer
 * Copyright (C) 2013  Imagination Technologies Ltd.
 */
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/tick.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/export.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/personality.h>
#include <linux/sys.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/kallsyms.h>
#include <linux/random.h>
#include <linux/prctl.h>

#include <asm/asm.h>
#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <asm/dsp.h>
#include <asm/fpu.h>
#include <asm/msa.h>
#include <asm/pgtable.h>
#include <asm/mipsregs.h>
#include <asm/processor.h>
#include <asm/reg.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/elf.h>
#include <asm/isadep.h>
#include <asm/inst.h>
#include <asm/stacktrace.h>
#include <asm/irq_regs.h>

#ifdef CONFIG_HOTPLUG_CPU
void arch_cpu_idle_dead(void)
{
	/* What the heck is this check doing ? */
	if (!cpumask_test_cpu(smp_processor_id(), &cpu_callin_map))
		play_dead();
}
#endif

asmlinkage void ret_from_fork(void);
asmlinkage void ret_from_kernel_thread(void);

void start_thread(struct pt_regs * regs, unsigned long pc, unsigned long sp)
{
	unsigned long status;

	/* New thread loses kernel privileges. */
	status = regs->cp0_status & ~(ST0_CU0|ST0_CU1|ST0_FR|KU_MASK);
	status |= KU_USER;
	regs->cp0_status = status;
	lose_fpu(0);
	clear_thread_flag(TIF_MSA_CTX_LIVE);
	clear_used_math();
	init_dsp();
	regs->cp0_epc = pc;
	regs->regs[29] = sp;
}

void exit_thread(void)
{
}

void flush_thread(void)
{
}

int arch_dup_task_struct(struct task_struct *dst, struct task_struct *src)
{
	/*
	 * Save any process state which is live in hardware registers to the
	 * parent context prior to duplication. This prevents the new child
	 * state becoming stale if the parent is preempted before copy_thread()
	 * gets a chance to save the parent's live hardware registers to the
	 * child context.
	 */
	preempt_disable();

	if (is_msa_enabled())
		save_msa(current);
	else if (is_fpu_owner())
		_save_fp(current);

	save_dsp(current);

	preempt_enable();

	*dst = *src;
	return 0;
}

/*
 * Copy architecture-specific thread state
 */
int copy_thread(unsigned long clone_flags, unsigned long usp,
	unsigned long kthread_arg, struct task_struct *p)
{
	struct thread_info *ti = task_thread_info(p);
	struct pt_regs *childregs, *regs = current_pt_regs();
	unsigned long childksp;
	p->set_child_tid = p->clear_child_tid = NULL;

	childksp = (unsigned long)task_stack_page(p) + THREAD_SIZE - 32;

	/* set up new TSS. */
	childregs = (struct pt_regs *) childksp - 1;
	/*  Put the stack after the struct pt_regs.  */
	childksp = (unsigned long) childregs;
	p->thread.cp0_status = read_c0_status() & ~(ST0_CU2|ST0_CU1);
	if (unlikely(p->flags & PF_KTHREAD)) {
		/* kernel thread */
		unsigned long status = p->thread.cp0_status;
		memset(childregs, 0, sizeof(struct pt_regs));
		ti->addr_limit = KERNEL_DS;
		p->thread.reg16 = usp; /* fn */
		p->thread.reg17 = kthread_arg;
		p->thread.reg29 = childksp;
		p->thread.reg31 = (unsigned long) ret_from_kernel_thread;
#if defined(CONFIG_CPU_R3000) || defined(CONFIG_CPU_TX39XX)
		status = (status & ~(ST0_KUP | ST0_IEP | ST0_IEC)) |
			 ((status & (ST0_KUC | ST0_IEC)) << 2);
#else
		status |= ST0_EXL;
#endif
		childregs->cp0_status = status;
		return 0;
	}

	/* user thread */
	*childregs = *regs;
	childregs->regs[7] = 0; /* Clear error flag */
	childregs->regs[2] = 0; /* Child gets zero as return value */
	if (usp)
		childregs->regs[29] = usp;
	ti->addr_limit = USER_DS;

	p->thread.reg29 = (unsigned long) childregs;
	p->thread.reg31 = (unsigned long) ret_from_fork;

	/*
	 * New tasks lose permission to use the fpu. This accelerates context
	 * switching for most programs since they don't use the fpu.
	 */
	childregs->cp0_status &= ~(ST0_CU2|ST0_CU1);

	clear_tsk_thread_flag(p, TIF_USEDFPU);
	clear_tsk_thread_flag(p, TIF_USEDMSA);
	clear_tsk_thread_flag(p, TIF_MSA_CTX_LIVE);

#ifdef CONFIG_MIPS_MT_FPAFF
	clear_tsk_thread_flag(p, TIF_FPUBOUND);
#endif /* CONFIG_MIPS_MT_FPAFF */

	if (clone_flags & CLONE_SETTLS)
		ti->tp_value = regs->regs[7];

	return 0;
}

#ifdef CONFIG_CC_STACKPROTECTOR
#include <linux/stackprotector.h>
unsigned long __stack_chk_guard __read_mostly;
EXPORT_SYMBOL(__stack_chk_guard);
#endif

struct mips_frame_info {
	void		*func;
	unsigned long	func_size;
	int		frame_size;
	int		pc_offset;
};

#define J_TARGET(pc,target)	\
		(((unsigned long)(pc) & 0xf0000000) | ((target) << 2))

static inline int is_ra_save_ins(union mips_instruction *ip)
{
#ifdef CONFIG_CPU_MICROMIPS
	union mips_instruction mmi;

	/*
	 * swsp ra,offset
	 * swm16 reglist,offset(sp)
	 * swm32 reglist,offset(sp)
	 * sw32 ra,offset(sp)
	 * jradiussp - NOT SUPPORTED
	 *
	 * microMIPS is way more fun...
	 */
	if (mm_insn_16bit(ip->halfword[0])) {
		mmi.word = (ip->halfword[0] << 16);
		return (mmi.mm16_r5_format.opcode == mm_swsp16_op &&
			mmi.mm16_r5_format.rt == 31) ||
		       (mmi.mm16_m_format.opcode == mm_pool16c_op &&
			mmi.mm16_m_format.func == mm_swm16_op);
	}
	else {
		mmi.halfword[0] = ip->halfword[1];
		mmi.halfword[1] = ip->halfword[0];
		return (mmi.mm_m_format.opcode == mm_pool32b_op &&
			mmi.mm_m_format.rd > 9 &&
			mmi.mm_m_format.base == 29 &&
			mmi.mm_m_format.func == mm_swm32_func) ||
		       (mmi.i_format.opcode == mm_sw32_op &&
			mmi.i_format.rs == 29 &&
			mmi.i_format.rt == 31);
	}
#else
	/* sw / sd $ra, offset($sp) */
	return (ip->i_format.opcode == sw_op || ip->i_format.opcode == sd_op) &&
		ip->i_format.rs == 29 &&
		ip->i_format.rt == 31;
#endif
}

static inline int is_jump_ins(union mips_instruction *ip)
{
#ifdef CONFIG_CPU_MICROMIPS
	/*
	 * jr16,jrc,jalr16,jalr16
	 * jal
	 * jalr/jr,jalr.hb/jr.hb,jalrs,jalrs.hb
	 * jraddiusp - NOT SUPPORTED
	 *
	 * microMIPS is kind of more fun...
	 */
	union mips_instruction mmi;

	mmi.word = (ip->halfword[0] << 16);

	if ((mmi.mm16_r5_format.opcode == mm_pool16c_op &&
	    (mmi.mm16_r5_format.rt & mm_jr16_op) == mm_jr16_op) ||
	    ip->j_format.opcode == mm_jal32_op)
		return 1;
	if (ip->r_format.opcode != mm_pool32a_op ||
			ip->r_format.func != mm_pool32axf_op)
		return 0;
	return ((ip->u_format.uimmediate >> 6) & mm_jalr_op) == mm_jalr_op;
#else
	if (ip->j_format.opcode == j_op)
		return 1;
	if (ip->j_format.opcode == jal_op)
		return 1;
	if (ip->r_format.opcode != spec_op)
		return 0;
	return ip->r_format.func == jalr_op || ip->r_format.func == jr_op;
#endif
}

static inline int is_sp_move_ins(union mips_instruction *ip)
{
#ifdef CONFIG_CPU_MICROMIPS
	/*
	 * addiusp -imm
	 * addius5 sp,-imm
	 * addiu32 sp,sp,-imm
	 * jradiussp - NOT SUPPORTED
	 *
	 * microMIPS is not more fun...
	 */
	if (mm_insn_16bit(ip->halfword[0])) {
		union mips_instruction mmi;

		mmi.word = (ip->halfword[0] << 16);
		return (mmi.mm16_r3_format.opcode == mm_pool16d_op &&
			mmi.mm16_r3_format.simmediate && mm_addiusp_func) ||
		       (mmi.mm16_r5_format.opcode == mm_pool16d_op &&
			mmi.mm16_r5_format.rt == 29);
	}
	return ip->mm_i_format.opcode == mm_addiu32_op &&
	       ip->mm_i_format.rt == 29 && ip->mm_i_format.rs == 29;
#else
	/* addiu/daddiu sp,sp,-imm */
	if (ip->i_format.rs != 29 || ip->i_format.rt != 29)
		return 0;
	if (ip->i_format.opcode == addiu_op || ip->i_format.opcode == daddiu_op)
		return 1;
#endif
	return 0;
}

static int get_frame_info(struct mips_frame_info *info)
{
#ifdef CONFIG_CPU_MICROMIPS
	union mips_instruction *ip = (void *) (((char *) info->func) - 1);
#else
	union mips_instruction *ip = info->func;
#endif
	unsigned max_insns = info->func_size / sizeof(union mips_instruction);
	unsigned i;

	info->pc_offset = -1;
	info->frame_size = 0;

	if (!ip)
		goto err;

	if (max_insns == 0)
		max_insns = 128U;	/* unknown function size */
	max_insns = min(128U, max_insns);

	for (i = 0; i < max_insns; i++, ip++) {

		if (is_jump_ins(ip))
			break;
		if (!info->frame_size) {
			if (is_sp_move_ins(ip))
			{
#ifdef CONFIG_CPU_MICROMIPS
				if (mm_insn_16bit(ip->halfword[0]))
				{
					unsigned short tmp;

					if (ip->halfword[0] & mm_addiusp_func)
					{
						tmp = (((ip->halfword[0] >> 1) & 0x1ff) << 2);
						info->frame_size = -(signed short)(tmp | ((tmp & 0x100) ? 0xfe00 : 0));
					} else {
						tmp = (ip->halfword[0] >> 1);
						info->frame_size = -(signed short)(tmp & 0xf);
					}
					ip = (void *) &ip->halfword[1];
					ip--;
				} else
#endif
				info->frame_size = - ip->i_format.simmediate;
			}
			continue;
		}
		if (info->pc_offset == -1 && is_ra_save_ins(ip)) {
			info->pc_offset =
				ip->i_format.simmediate / sizeof(long);
			break;
		}
	}
	if (info->frame_size && info->pc_offset >= 0) /* nested */
		return 0;
	if (info->pc_offset < 0) /* leaf */
		return 1;
	/* prologue seems boggus... */
err:
	return -1;
}

static struct mips_frame_info schedule_mfi __read_mostly;

#ifdef CONFIG_KALLSYMS
static unsigned long get___schedule_addr(void)
{
	return kallsyms_lookup_name("__schedule");
}
#else
static unsigned long get___schedule_addr(void)
{
	union mips_instruction *ip = (void *)schedule;
	int max_insns = 8;
	int i;

	for (i = 0; i < max_insns; i++, ip++) {
		if (ip->j_format.opcode == j_op)
			return J_TARGET(ip, ip->j_format.target);
	}
	return 0;
}
#endif

static int __init frame_info_init(void)
{
	unsigned long size = 0;
#ifdef CONFIG_KALLSYMS
	unsigned long ofs;
#endif
	unsigned long addr;

	addr = get___schedule_addr();
	if (!addr)
		addr = (unsigned long)schedule;

#ifdef CONFIG_KALLSYMS
	kallsyms_lookup_size_offset(addr, &size, &ofs);
#endif
	schedule_mfi.func = (void *)addr;
	schedule_mfi.func_size = size;

	get_frame_info(&schedule_mfi);

	/*
	 * Without schedule() frame info, result given by
	 * thread_saved_pc() and get_wchan() are not reliable.
	 */
	if (schedule_mfi.pc_offset < 0)
		printk("Can't analyze schedule() prologue at %p\n", schedule);

	return 0;
}

arch_initcall(frame_info_init);

/*
 * Return saved PC of a blocked thread.
 */
unsigned long thread_saved_pc(struct task_struct *tsk)
{
	struct thread_struct *t = &tsk->thread;

	/* New born processes are a special case */
	if (t->reg31 == (unsigned long) ret_from_fork)
		return t->reg31;
	if (schedule_mfi.pc_offset < 0)
		return 0;
	return ((unsigned long *)t->reg29)[schedule_mfi.pc_offset];
}


#ifdef CONFIG_KALLSYMS
/* generic stack unwinding function */
unsigned long notrace unwind_stack_by_address(unsigned long stack_page,
					      unsigned long *sp,
					      unsigned long pc,
					      unsigned long *ra)
{
	struct mips_frame_info info;
	unsigned long size, ofs;
	int leaf;
	extern void ret_from_irq(void);
	extern void ret_from_exception(void);

	if (!stack_page)
		return 0;

	/*
	 * If we reached the bottom of interrupt context,
	 * return saved pc in pt_regs.
	 */
	if (pc == (unsigned long)ret_from_irq ||
	    pc == (unsigned long)ret_from_exception) {
		struct pt_regs *regs;
		if (*sp >= stack_page &&
		    *sp + sizeof(*regs) <= stack_page + THREAD_SIZE - 32) {
			regs = (struct pt_regs *)*sp;
			pc = regs->cp0_epc;
			if (!user_mode(regs) && __kernel_text_address(pc)) {
				*sp = regs->regs[29];
				*ra = regs->regs[31];
				return pc;
			}
		}
		return 0;
	}
	if (!kallsyms_lookup_size_offset(pc, &size, &ofs))
		return 0;
	/*
	 * Return ra if an exception occurred at the first instruction
	 */
	if (unlikely(ofs == 0)) {
		pc = *ra;
		*ra = 0;
		return pc;
	}

	info.func = (void *)(pc - ofs);
	info.func_size = ofs;	/* analyze from start to ofs */
	leaf = get_frame_info(&info);
	if (leaf < 0)
		return 0;

	if (*sp < stack_page ||
	    *sp + info.frame_size > stack_page + THREAD_SIZE - 32)
		return 0;

	if (leaf)
		/*
		 * For some extreme cases, get_frame_info() can
		 * consider wrongly a nested function as a leaf
		 * one. In that cases avoid to return always the
		 * same value.
		 */
		pc = pc != *ra ? *ra : 0;
	else
		pc = ((unsigned long *)(*sp))[info.pc_offset];

	*sp += info.frame_size;
	*ra = 0;
	return __kernel_text_address(pc) ? pc : 0;
}
EXPORT_SYMBOL(unwind_stack_by_address);

/* used by show_backtrace() */
unsigned long unwind_stack(struct task_struct *task, unsigned long *sp,
			   unsigned long pc, unsigned long *ra)
{
	unsigned long stack_page = (unsigned long)task_stack_page(task);
	return unwind_stack_by_address(stack_page, sp, pc, ra);
}
#endif

/*
 * get_wchan - a maintenance nightmare^W^Wpain in the ass ...
 */
unsigned long get_wchan(struct task_struct *task)
{
	unsigned long pc = 0;
#ifdef CONFIG_KALLSYMS
	unsigned long sp;
	unsigned long ra = 0;
#endif

	if (!task || task == current || task->state == TASK_RUNNING)
		goto out;
	if (!task_stack_page(task))
		goto out;

	pc = thread_saved_pc(task);

#ifdef CONFIG_KALLSYMS
	sp = task->thread.reg29 + schedule_mfi.frame_size;

	while (in_sched_functions(pc))
		pc = unwind_stack(task, &sp, pc, &ra);
#endif

out:
	return pc;
}

static void arch_dump_stack(void *info)
{
	struct pt_regs *regs;

	regs = get_irq_regs();

	if (regs)
		show_regs(regs);

	dump_stack();
}

void arch_trigger_all_cpu_backtrace(bool include_self)
{
	smp_call_function(arch_dump_stack, NULL, 1);
}

int mips_get_process_fp_mode(struct task_struct *task)
{
	int value = 0;

	if (!test_tsk_thread_flag(task, TIF_32BIT_FPREGS))
		value |= PR_FP_MODE_FR;
	if (test_tsk_thread_flag(task, TIF_HYBRID_FPREGS))
		value |= PR_FP_MODE_FRE;

	return value;
}

static void prepare_for_fp_mode_switch(void *info)
{
	struct mm_struct *mm = info;

	if (current->mm == mm)
		lose_fpu(1);
}

int mips_set_process_fp_mode(struct task_struct *task, unsigned int value)
{
	const unsigned int known_bits = PR_FP_MODE_FR | PR_FP_MODE_FRE;
	struct task_struct *t;
	int max_users;

	/* Check the value is valid */
	if (value & ~known_bits)
		return -EOPNOTSUPP;

	/* Avoid inadvertently triggering emulation */
	if ((value & PR_FP_MODE_FR) && cpu_has_fpu &&
	    !(current_cpu_data.fpu_id & MIPS_FPIR_F64))
		return -EOPNOTSUPP;
	if ((value & PR_FP_MODE_FRE) && cpu_has_fpu && !cpu_has_fre)
		return -EOPNOTSUPP;

	/* FR = 0 not supported in MIPS R6 */
	if (!(value & PR_FP_MODE_FR) && cpu_has_fpu && cpu_has_mips_r6)
		return -EOPNOTSUPP;

	/* Proceed with the mode switch */
	preempt_disable();

	/* Save FP & vector context, then disable FPU & MSA */
	if (task->signal == current->signal)
		lose_fpu(1);

	/* Prevent any threads from obtaining live FP context */
	atomic_set(&task->mm->context.fp_mode_switching, 1);
	smp_mb__after_atomic();

	/*
	 * If there are multiple online CPUs then force any which are running
	 * threads in this process to lose their FPU context, which they can't
	 * regain until fp_mode_switching is cleared later.
	 */
	if (num_online_cpus() > 1) {
		/* No need to send an IPI for the local CPU */
		max_users = (task->mm == current->mm) ? 1 : 0;

		if (atomic_read(&current->mm->mm_users) > max_users)
			smp_call_function(prepare_for_fp_mode_switch,
					  (void *)current->mm, 1);
	}

	/*
	 * There are now no threads of the process with live FP context, so it
	 * is safe to proceed with the FP mode switch.
	 */
	for_each_thread(task, t) {
		/* Update desired FP register width */
		if (value & PR_FP_MODE_FR) {
			clear_tsk_thread_flag(t, TIF_32BIT_FPREGS);
		} else {
			set_tsk_thread_flag(t, TIF_32BIT_FPREGS);
			clear_tsk_thread_flag(t, TIF_MSA_CTX_LIVE);
		}

		/* Update desired FP single layout */
		if (value & PR_FP_MODE_FRE)
			set_tsk_thread_flag(t, TIF_HYBRID_FPREGS);
		else
			clear_tsk_thread_flag(t, TIF_HYBRID_FPREGS);
	}

	/* Allow threads to use FP again */
	atomic_set(&task->mm->context.fp_mode_switching, 0);
	preempt_enable();

	return 0;
}
