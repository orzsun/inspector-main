// Purpose: Registers native game functions as API endpoints for the embedded web browser (e.g., Trading Post, Gem Store).
// These functions allow JavaScript within the web view to call back into the game's C++ code.
//
// Registered Endpoints include:
// - Trading Post/Economy: GetBuildInfo, QueryItemInfo, SellItem, BuyFeature, IsTradingPostActive
// - Account/Social: AccountGetContacts, AccountGetGuilds, CharacterGetParty, GetStats
// - UI/Tooltips: ShowItemContextMenu, HideItemContextMenu, ShowItemTooltip, HideItemTooltip, ShowTextTooltip, HideTextTooltip, ChangeTab, SetLoading
// - Character Preview: CharacterShow, CharacterHide, CharacterSetPreviewItem, CharacterClearPreviewItems, CharacterSetPreviewOutfit, CharacterClearPreviewOutfit
// - Chat: ChatPost, ChatSend, ChatItemSend, ChatItemInsert
// - Browser/Error Handling: HasBrowserCrashed, ShowInDefaultBrowser, ShowNetErrorBasic
// - Secure Token Service (STS): StsRequest
//
// This function also queues an event (0x28) via Event::Factory_QueueEvent at the end.
void ArenaWeb::RegisterApiEndpoints(longlong param_1)
{
  uint uVar1;
  
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xd8))
            (*(longlong **)(param_1 + 0xb8),L"GetBuildInfo",Arena::Engine::ArenaWeb::ArenaWeb,
             param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xd8))
            (*(longlong **)(param_1 + 0xb8),L"QueryItemInfo",Arena::Engine::ArenaWeb::ArenaWeb,
             param_1,2);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xd8))
            (*(longlong **)(param_1 + 0xb8),L"SellItem",Arena::Engine::ArenaWeb::ArenaWeb,param_1,2)
  ;
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xd8))
            (*(longlong **)(param_1 + 0xb8),L"GetTimeFromRequest",Arena::Engine::ArenaWeb::ArenaWeb,
             param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xd8))
            (*(longlong **)(param_1 + 0xb8),L"HasBrowserCrashed",Arena::Engine::ArenaWeb::ArenaWeb,
             param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xd8))
            (*(longlong **)(param_1 + 0xb8),L"IsTradingPostActive",Arena::Engine::ArenaWeb::ArenaWeb
             ,param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xd8))
            (*(longlong **)(param_1 + 0xb8),L"AccountGetContacts",Arena::Engine::ArenaWeb::ArenaWeb,
             param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xd8))
            (*(longlong **)(param_1 + 0xb8),L"AccountGetGuilds",Arena::Engine::ArenaWeb::ArenaWeb,
             param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xd8))
            (*(longlong **)(param_1 + 0xb8),L"CharacterGetParty",Arena::Engine::ArenaWeb::ArenaWeb,
             param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xd8))
            (*(longlong **)(param_1 + 0xb8),L"GetStats",Arena::Engine::ArenaWeb::ArenaWeb,param_1,1)
  ;
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"BuyFeature",Arena::Engine::ArenaWeb::ArenaWeb,param_1,
             1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"ShowItemContextMenu",Arena::Engine::ArenaWeb::ArenaWeb
             ,param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"HideItemContextMenu",Arena::Engine::ArenaWeb::ArenaWeb
             ,param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"ShowItemTooltip",Arena::Engine::ArenaWeb::ArenaWeb,
             param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"HideItemTooltip",Arena::Engine::ArenaWeb::ArenaWeb,
             param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"ShowTextTooltip",Arena::Engine::ArenaWeb::ArenaWeb,
             param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"HideTextTooltip",Arena::Engine::ArenaWeb::ArenaWeb,
             param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"ShowInDefaultBrowser",
             Arena::Engine::ArenaWeb::ArenaWeb,param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"ShowNetErrorBasic",Arena::Engine::ArenaWeb::ArenaWeb,
             param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"CharacterShow",Arena::Engine::ArenaWeb::ArenaWeb,
             param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"CharacterHide",Arena::Engine::ArenaWeb::ArenaWeb,
             param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"CharacterSetPreviewItem",
             Arena::Engine::ArenaWeb::ArenaWeb,param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"CharacterClearPreviewItems",
             Arena::Engine::ArenaWeb::ArenaWeb,param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"CharacterSetPreviewOutfit",
             Arena::Engine::ArenaWeb::ArenaWeb,param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"CharacterClearPreviewOutfit",
             Arena::Engine::ArenaWeb::ArenaWeb,param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"ChangeTab",Arena::Engine::ArenaWeb::ArenaWeb,param_1,1
            );
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"SetLoading",Arena::Engine::ArenaWeb::ArenaWeb,param_1,
             1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"SetTextEncryptionKey",
             Arena::Engine::ArenaWeb::ArenaWeb,param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"PickupAllItems",Arena::Engine::ArenaWeb::ArenaWeb,
             param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"ChatPost",Arena::Engine::ArenaWeb::ArenaWeb,param_1,1)
  ;
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"ChatSend",Arena::Engine::ArenaWeb::ArenaWeb,param_1,1)
  ;
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"ChatItemSend",Arena::Engine::ArenaWeb::ArenaWeb,
             param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xe0))
            (*(longlong **)(param_1 + 0xb8),L"ChatItemInsert",Arena::Engine::ArenaWeb::ArenaWeb,
             param_1,1);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xd8))
            (*(longlong **)(param_1 + 0xb8),L"StsRequest",Arena::Engine::ArenaWeb::ArenaWeb,param_1,
             2);
  (**(code **)(**(longlong **)(param_1 + 0xb8) + 0xd0))();
  uVar1 = FUN_1409a96a0();
                    /* WARNING: Subroutine does not return */
  Event::Factory_QueueEvent(1,0x28,0,0,(char *)0x0,0,uVar1);
}

