#define SLIMR_TEST_MAIN
#include "test.hpp"

// Phase 1: Diff & Model tests
#include "diff/test_myers.hpp"
#include "diff/test_unified.hpp"
#include "model/test_style_values.hpp"
#include "model/test_symbols.hpp"
#include "model/test_object_ids.hpp"
#include "model/test_lists.hpp"
#include "model/test_layout.hpp"
#include "model/test_from_json.hpp"
#include "model/test_equivalence.hpp"

// Phase 2: In-Memory Emulator & Conformance tests
#include "emulator/test_emulate.hpp"
#include "emulator/test_emulate_structure.hpp"
#include "emulator/test_emulate_tables.hpp"
#include "emulator/test_fake_google.hpp"
#include "emulator/test_conformance.hpp"

// Phase 3: Markdown AST Lens tests
#include "lens/test_lens_all.hpp"
#include "lens/test_links.hpp"
#include "lens/test_sections.hpp"

// Phase 4: Reconciler tests
#include "reconcile/test_paragraph.hpp"
#include "reconcile/test_lists.hpp"
#include "reconcile/test_tables.hpp"
#include "reconcile/test_container.hpp"
#include "reconcile/test_reconcile.hpp"
#include "reconcile/test_fuzz.hpp"

// Phase 5: Engine tests
#include "engine/test_artifacts.hpp"
#include "engine/test_load.hpp"
#include "engine/test_guard.hpp"
#include "engine/test_export.hpp"
#include "engine/test_import.hpp"
// NOTE(B3): test_transaction.hpp is blocked on B4 — src/engine/transaction.hpp does not
// compile yet (undefined DocPhaseReceipt/TransactionErrorReceipts/linkRequests/verify/
// VerifyResolvedOptions). Uncomment once B4 fixes the transaction runner.
// #include "engine/test_transaction.hpp"

// Phase 6: API client tests
#include "api/test_api.hpp"
