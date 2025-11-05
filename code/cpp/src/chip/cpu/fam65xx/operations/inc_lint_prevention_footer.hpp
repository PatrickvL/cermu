/*
 * inc_lint_prevention_footer.hpp - Footer for Lint Prevention System
 * 
 * This footer closes the surrogate class context and restores compiler 
 * diagnostics. It must be included at the end of each .inc.hpp file
 * after all member function implementations.
 *
 * DESIGN:
 * =======
 * - Closes the surrogate template class opened in inc_lint_prevention.hpp
 * - Closes the namespace context
 * - Restores compiler diagnostic settings
 * - Only active during standalone analysis (when FAM65XX_TEMPLATE_CONTEXT is not defined)
 */

#ifndef FAM65XX_TEMPLATE_CONTEXT
  // This section only applies during standalone lint analysis
  
  // =========================================================================
  // CLOSE SURROGATE CLASS AND NAMESPACE CONTEXT
  // =========================================================================
  
    }; // End of template<typename ProcessorTag> class fam65xx_t
    
  } // End of namespace fam65xx

  // =========================================================================
  // RESTORE COMPILER DIAGNOSTICS
  // =========================================================================
  
  #if defined(__clang__)
    #pragma clang diagnostic pop
  #elif defined(__GNUC__)
    #pragma GCC diagnostic pop
  #elif defined(_MSC_VER)
    #pragma warning(pop)
  #endif

#endif // FAM65XX_TEMPLATE_CONTEXT