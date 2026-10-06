// DragOutTokens.h - the component's one drag-out token store.
#pragma once

#include "webview/dnd/DragTokenStore.h"

namespace fb2k_dnd {

// The single store every window mints from and every drag redeems against.
//
// Kept apart from DragTokenStore itself so that class stays free of Win32: the
// production clock and the cryptographic random source live here, and this file
// is built only into the component, never into the unit-test target.
//
// Main thread only, like the store it hands back.
DragTokenStore& DragOutTokenStore();

}  // namespace fb2k_dnd
