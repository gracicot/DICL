#include <kangaru/kangaru.hpp>
#include <iostream>

/**
 * This example reflects snippets of code found in the documentation section 2: Invoke
 * It explains how to do service injection through the parameters of a function.
 */

// Uncomment this to receive the scene in process_inputs_mod
// #define HAS_SCENE_PARAMETER

struct KeyboardState {};
struct MessageBus {};
struct Camera {};

// This is the definition for our classes
struct KeyboardStateService : kgr::single_service<KeyboardState> {};
struct MessageBusService    : kgr::single_service<MessageBus> {};
struct CameraService        : kgr::service<Camera> {};

// This is the service map. We map a parameter type to a service definition.
auto service_map(KeyboardState const&) -> KeyboardStateService;
auto service_map(MessageBus const&)    -> MessageBusService;
auto service_map(Camera const&)        -> CameraService;

// These are functions we will call with invoke
bool process_inputs(KeyboardState& ks, MessageBus& mb) {
	std::cout << "processing inputs...\n";
	return true;
}

#ifndef HAS_SCENE_PARAMETER

	bool process_inputs_mod(KeyboardState& ks, MessageBus& mb, bool check_modifiers) {
		std::cout << "process inputs " << (check_modifiers ? "with" : "without") << " modifiers\n";
		return !check_modifiers;
	}

#else

	bool process_inputs_mod(KeyboardState& ks, MessageBus& mb, Camera scene, bool check_modifiers) {
		std::cout << "process inputs " << (check_modifiers ? "with" : "without") << " modifiers\n";
		std::cout << "got the scene!\n";
		return !check_modifiers;
	}

#endif

int main()
{
	kgr::container container;
	
	
	// We invoke a function specifying services
	bool result1 = container.invoke<KeyboardStateService, MessageBusService>(process_inputs);
	
	// The code above is equivalent to this:
	//
	//    process_inputs(
	//        container.service<KeyboardStateService>(),
	//        container.service<MessageBusService>()
	//    );
	//
	
	// We can also let the container match parameters itself with the service map.
	bool result2 = container.invoke(process_inputs);
	bool result3 = container.invoke(process_inputs_mod, true);
	
	std::cout << '\n';
	
	// 1: true, 2: true, 3: false
	std::cout << "Invoke results: \n  1: " << result1;
	std::cout << "\n  2: " << result2;
	std::cout << "\n  3: " << result3 << '\n';
	
	// We call a lambda with injected parameter using invoke.
	container.invoke([](Camera camera) {
		std::cout << "Lambda called." << std::endl;
	});
}
