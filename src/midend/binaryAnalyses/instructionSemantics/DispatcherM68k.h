#ifndef ROSE_DispatcherM68k_H
#define ROSE_DispatcherM68k_H

#include "BaseSemantics2.h"

namespace BinaryAnalysis {
namespace InstructionSemantics2 {

typedef boost::shared_ptr<class DispatcherM68k> DispatcherM68kPtr;

class DispatcherM68k: public BaseSemantics::Dispatcher {
protected:
    explicit DispatcherM68k(const BaseSemantics::RiscOperatorsPtr &ops): BaseSemantics::Dispatcher(ops) {
        set_register_dictionary(RegisterDictionary::dictionary_coldfire());
        regcache_init();
        iproc_init();
    }

    /** Loads the iproc table with instruction processing functors. This normally happens from the constructor. */
    void iproc_init();

    /** Load the cached register descriptors.  This happens at construction and on set_register_dictionary() calls. */
    void regcache_init();

public:
    /** Cached register.
     *
     *  This register is cached so that there are not so many calls to Dispatcher::findRegister(). Changing the register
     *  dictionary via set_register_dictionary() invalidates all entries of the cache.
     *
     * @{ */
    RegisterDescriptor REG_D[8], REG_A[8], REG_FP[8], REG_PC;
    /** @} */

    /** Constructor. */
    static DispatcherM68kPtr instance(const BaseSemantics::RiscOperatorsPtr &ops) {
        return DispatcherM68kPtr(new DispatcherM68k(ops));
    }

    /** Virtual constructor. */
    virtual BaseSemantics::DispatcherPtr create(const BaseSemantics::RiscOperatorsPtr &ops) const /*override*/ {
        return instance(ops);
    }

    /** Dynamic cast to DispatcherM68kPtr with assertion. */
    static DispatcherM68kPtr promote(const BaseSemantics::DispatcherPtr &d) {
        DispatcherM68kPtr retval = boost::dynamic_pointer_cast<DispatcherM68k>(d);
        ASSERT_not_null(retval);
        return retval;
    }

    virtual void set_register_dictionary(const RegisterDictionary *regdict)/*override*/;

    virtual int iproc_key(SgAsmInstruction *insn_) const /*override*/ {
        SgAsmM68kInstruction *insn = isSgAsmM68kInstruction(insn_);
        ASSERT_not_null(insn);
        return insn->get_kind();
    }
};

} // namespace
} // namespace
#endif