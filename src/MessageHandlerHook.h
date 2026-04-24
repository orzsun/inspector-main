#pragma once

#include <cstdint>

/**
 * @brief Initializes inline hooks just before message handler calls within the dispatcher.
 * @param dispatcherFuncAddress The base address of the message dispatcher function (FUN_1412e9390).
 * @return true If hooks were successfully created and enabled, false otherwise.
 */
bool InitializeMessageHandlerHooks(uintptr_t dispatcherFuncAddress);

/**
 * @brief Disables and removes the message handler inline hooks.
 */
void CleanupMessageHandlerHooks();