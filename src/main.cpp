SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    Init(skse);

    auto g_task = SKSE::GetTaskInterface();
	auto g_message = SKSE::GetMessagingInterface();
	g_message->RegisterListener([](SKSE::MessagingInterface::Message* msg) -> void {
		if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
            RE::PlayerCharacter *p = RE::PlayerCharacter::GetSingleton();
			RE::UserEvents *ue = RE::UserEvents::GetSingleton();
            
            /*if (current ue = ue->attackPowerStart) {
                    if (p.iscrouching || p.isSprinting) {
                        trigger vanilla actions
                    }
                    
                    while player is holding power attack button {
                        drain stamina at a fixed rate (possibly customisable with MCM?)
                        scale power attack damage at a fixed rate (possibly customisable with MCM?)
                    }

                    play attack animation
                }*/ 
        }
    });
    return true;
}