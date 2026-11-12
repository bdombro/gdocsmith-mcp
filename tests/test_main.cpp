#define SLIMR_TEST_MAIN
#include "test.hpp"
#include "test_engine.hpp"
#include "test_storage.hpp"

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

// Phase 6: API client tests
#include "api/test_api.hpp"
