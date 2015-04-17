#pragma once

#include <unordered_map>
#include <memory>
#include <type_traits>
#include <tuple>

namespace kgr {

template<typename... Types>
struct Dependency {
	using DependenciesTypes = std::tuple<Types...>;
};

using NoDependencies = Dependency<>;

template<typename T>
struct Service;

namespace detail {

enum class enabler {};

template <bool b, typename T>
using enable_if_t = typename std::enable_if<b, T>::type;

template<int ...>
struct seq {};

template<int n, int ...S>
struct seq_gen : seq_gen<n-1, n-1, S...> {};

template<int ...S>
struct seq_gen<0, S...> {
	using type = seq<S...>;
};

template <typename U>
struct function_traits
	: function_traits<decltype(&U::operator())>
{};

template <typename Type, typename R, typename... Args>
struct function_traits<R(Type::*)(Args...) const> {
	using return_type = R;
	using argument_types = std::tuple<Args...>;
};

template <typename Type, typename R, typename... Args>
struct function_traits<R(Type::*)(Args...)> {
	using return_type = R;
	using argument_types = std::tuple<Args...>;
};

template <typename R, typename... Args>
struct function_traits<R(*)(Args...)> {
	using return_type = R;
	using argument_types = std::tuple<Args...>;
};

struct Holder {
	virtual ~Holder() = default;
};

template<typename T>
struct InstanceHolder final : Holder {
	explicit InstanceHolder(std::shared_ptr<T> instance) : _instance{std::move(instance)} {}
	
	std::shared_ptr<T> getInstance() const {
		return _instance;
	}
	
private:
	std::shared_ptr<T> _instance;
};

template<typename T, typename... Args>
struct CallbackHolder final : Holder {
	using callback_t = std::function<T(Args...)>;

	explicit CallbackHolder(callback_t callback) : _callback{std::move(callback)} {}
	
	T operator ()(Args... args) {
		return _callback(args...);
	}
private:
	callback_t _callback;
};

template<typename T, typename ...Args>
std::unique_ptr<T> make_unique( Args&& ...args )
{
	return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template <typename T, typename ...Args> void type_id() {}
using type_id_fn = void(*)();

template<typename T>
struct check_pointer_type {
private:
	using yes = char;
	using no = struct { char array[2]; };

	template<typename C> static yes test(typename Service<C>::template PointerType<C>*);
	template<typename C> static no test(...);
	
public:
	constexpr static bool value = sizeof(test<T>(0)) == sizeof(yes);
};

template<typename Ptr>
struct PointerType {};

template<typename T>
struct PointerType<T*> {
	using Type = T*;
	using ServiceType = T;
	
	template<typename ...Args>
	static Type make_pointer(Args&&... args) {
		return new T(std::forward<Args>(args)...);
	}
};

template<typename T>
struct PointerType<std::shared_ptr<T>> {
	using Type = std::shared_ptr<T>;
	using ServiceType = T;
	
	template<typename ...Args>
	static Type make_pointer(Args&&... args) {
		return std::make_shared<T>(std::forward<Args>(args)...);
	}
};

template<typename T>
struct PointerType<std::unique_ptr<T>> {
	using Type = std::unique_ptr<T>;
	using ServiceType = T;
	
	template<typename ...Args>
	static Type make_pointer(Args&&... args) {
		return detail::make_unique<T>(std::forward<Args>(args)...);
	}
};

template <bool, typename T>
struct pointer_type_helper
{};

template <typename T>
struct pointer_type_helper<true, T> {
	using type = typename Service<T>::template PointerType<T>;
};

template <typename T>
struct pointer_type_helper<false, T> {
	using type = detail::PointerType<std::shared_ptr<T>>;
};

} // namespace detail

struct Single {
	using ParentTypes = std::tuple<>;
	template<typename T> using PointerType = detail::PointerType<std::shared_ptr<T>>;
};

struct Unique {
	template<typename T> using PointerType = detail::PointerType<std::unique_ptr<T>>;
};

struct Raw {
	template<typename T> using PointerType = detail::PointerType<T*>;
};

template<typename... Types>
struct Overrides : Single {
	using ParentTypes = std::tuple<Types...>;
};

class Container : public std::enable_shared_from_this<Container> { public:
	template<typename Condition, typename T = detail::enabler> using enable_if = detail::enable_if_t<Condition::value, T>;
	template<typename Condition, typename T = detail::enabler> using disable_if = detail::enable_if_t<!Condition::value, T>;
	template<typename T> using is_service_single = std::is_base_of<Single, Service<T>>;
	template<typename T> using is_abstract = std::is_abstract<T>;
	template<typename T> using is_base_of_container = std::is_base_of<Container, T>;
	template<typename T> using is_container = std::is_same<T, Container>;
	template<typename T> using dependency_types = typename Service<T>::DependenciesTypes;
	template<typename T> using parent_types = typename Service<T>::ParentTypes;
	template<typename Tuple> using tuple_seq = typename detail::seq_gen<std::tuple_size<Tuple>::value>::type;
	template<int S, typename T> using parent_element = typename std::tuple_element<S, parent_types<T>>::type;
	template<int S, typename Tuple> using tuple_element = typename std::tuple_element<S, Tuple>::type;
	using holder_ptr = std::unique_ptr<detail::Holder>;
	using holder_cont = std::unordered_map<detail::type_id_fn, holder_ptr>;
	template<typename T> using ptr_type_helper = typename detail::pointer_type_helper<detail::check_pointer_type<T>::value, T>::type;
	template<int S, typename Services> using ptr_type_helpers = ptr_type_helper<typename std::tuple_element<S, Services>::type>;
	template<typename T> using ptr_type = typename detail::pointer_type_helper<detail::check_pointer_type<T>::value, T>::type::Type;
	template<int S, typename Services> using ptr_types = ptr_type<typename std::tuple_element<S, Services>::type>;
	constexpr static detail::enabler null = {};
	
public:
	Container(const Container &) = delete;
	Container(Container &&) = delete;
	Container& operator =(const Container &) = delete;
	Container& operator =(Container &&) = delete;
	virtual ~Container() = default;

	template <typename T, typename... Args, 
		 enable_if<is_base_of_container<T>> = null>
	static std::shared_ptr<T> make_container(Args&&... args) {
		auto container = std::make_shared<T>(std::forward<Args>(args)...);
		static_cast<Container&>(*container).init();
		return container;
	}

	template<typename T>
	void instance(std::shared_ptr<T> service) {
		static_assert(is_service_single<T>::value, "instance only accept Single Service instance.");

		call_save_instance(std::move(service), tuple_seq<parent_types<T>>{});
	}
	
	template<typename T, typename ...Args>
	void instance(Args&& ...args) {
		static_assert(is_service_single<T>::value, "instance only accept Single Service instance.");

		instance(make_service<T>(std::forward<Args>(args)...));
	}
	
	template<typename T, typename ...Args, disable_if<is_abstract<T>> = null, disable_if<is_base_of_container<T>> = null>
	ptr_type<T> service(Args&& ...args) {
		return get_service<T>(std::forward<Args>(args)...);
	}
	
	template<typename T, enable_if<is_container<T>> = null>
	std::shared_ptr<T> service() {
		return shared_from_this();
	}
	
	template<typename T, disable_if<is_container<T>> = null, enable_if<is_base_of_container<T>> = null>
	std::shared_ptr<T> service() {
		return std::dynamic_pointer_cast<T>(shared_from_this());
	}
	
	template<typename T, enable_if<is_abstract<T>> = null>
	std::shared_ptr<T> service() {
		auto it = _services.find(&detail::type_id<T>);
		
		if (it != _services.end()) {
			return static_cast<detail::InstanceHolder<T>&>(*it->second).getInstance();
		}
		
		return {};
	}
	
	template<typename T, typename U>
	void callback(U callback) {
		using arguments = typename detail::function_traits<U>::argument_types;
		static_assert(!is_service_single<T>::value, "instance does not accept Single Service.");
		
		call_save_callback<T, arguments>(tuple_seq<dependency_types<T>>{}, callback);
	}
	
	template<typename U>
	void callback(U callback) {
		using T = typename detail::PointerType<typename detail::function_traits<U>::return_type>::ServiceType;
		using arguments = typename detail::function_traits<U>::argument_types;
		static_assert(!is_service_single<T>::value, "instance does not accept Single Service.");
		
		call_save_callback<T, arguments>(tuple_seq<dependency_types<T>>{}, callback);
	}
protected:
	Container() = default;
	virtual void init(){}	
private:
	template<typename T, enable_if<is_service_single<T>> = null>
	ptr_type<T> get_service() {
		auto it = _services.find(&detail::type_id<T>);
		
		if (it == _services.end()) {
			auto service = make_service<T>();
			instance(service);
			
			return service;
		} 
		return static_cast<detail::InstanceHolder<T>*>(it->second.get())->getInstance();
	}
	
	template<typename T, typename ...Args, disable_if<is_service_single<T>> = null>
	ptr_type<T> get_service(Args ...args) {
		return make_service<T>(std::forward<Args>(args)...);
	}

	template<typename T, int ...S>
	void call_save_instance(std::shared_ptr<T> service, detail::seq<S...>) {
		save_instance<T, parent_element<S, T>...>(std::move(service));
	}
	
	template<typename Tuple, int ...S>
	std::tuple<ptr_types<S, Tuple>...> dependency(detail::seq<S...>) {
		return std::make_tuple(service<tuple_element<S, Tuple>>()...);
	}
	
	template<typename T, typename Tuple, int ...S, typename ...Args>
	ptr_type<T> callback_make_service(detail::seq<S...>, Tuple dependencies, Args&& ...args) const {
		auto it = _callbacks.find(&detail::type_id<T, tuple_element<S, Tuple>..., Args...>);
		
		if (it != _callbacks.end()) {
			return static_cast<detail::CallbackHolder<ptr_type<T>, tuple_element<S, Tuple>..., Args...>&>(*it->second.get())(std::get<S>(dependencies)..., std::forward<Args>(args)...);
		}
		
		return {};
	}
	
	template<typename T, typename Tuple, int ...S, typename ...Args, enable_if<is_service_single<T>> = null>
	ptr_type<T> make_service_dependency(detail::seq<S...>, Tuple dependencies, Args&& ...args) const {
		return ptr_type_helper<T>::make_pointer(std::move(std::get<S>(dependencies))..., std::forward<Args>(args)...);
	}
	
	template<typename T, typename Tuple, int ...S, typename ...Args, disable_if<is_service_single<T>> = null>
	ptr_type<T> make_service_dependency(detail::seq<S...> seq, Tuple dependencies, Args&& ...args) const {
		auto service = callback_make_service<T, Tuple>(seq, dependencies, std::forward<Args>(args)...);
		
		return service ? service : std::make_shared<T>(std::get<S>(dependencies)..., std::forward<Args>(args)...);
	}

	template <typename T, typename ...Args>
	ptr_type<T> make_service(Args&& ...args) {
		auto seq = tuple_seq<dependency_types<T>>{};
		return make_service_dependency<T>(seq, dependency<dependency_types<T>>(seq), std::forward<Args>(args)...);
	}
	
	template<typename T, typename ...Others>
	detail::enable_if_t<(sizeof...(Others) > 0), void> save_instance(std::shared_ptr<T> service) {
		save_instance<T>(service);
		save_instance<Others...>(std::move(service));
	}
	
	template<typename T>
	void save_instance (std::shared_ptr<T> service) {
		_services[&detail::type_id<T>] = detail::make_unique<detail::InstanceHolder<T>>(std::move(service));
	}
	
	template<typename T, typename Tuple, int ...S, typename U>
	void call_save_callback(detail::seq<S...>, U callback) {
		using argument_dependency_types = std::tuple<tuple_element<S, Tuple>...>;
		using dependency_ptr = std::tuple<ptr_types<S, dependency_types<T>>...>;
		static_assert(std::is_same<dependency_ptr, argument_dependency_types>::value, "The callback should receive the dependencies in the right order as first parameters");
		
		save_callback<T, Tuple>(tuple_seq<Tuple>{}, callback);
	}
	
	template<typename T, typename Tuple, int ...S, typename U>
	void save_callback (detail::seq<S...>, U callback) {
		_callbacks[&detail::type_id<T, tuple_element<S, Tuple>...>] = detail::make_unique<detail::CallbackHolder<ptr_type<T>, tuple_element<S, Tuple>...>>(callback);
	}
	
	holder_cont _callbacks;
	holder_cont _services;
};

template<typename T = Container, typename ...Args>
std::shared_ptr<T> make_container(Args&& ...args) {
	static_assert(std::is_base_of<Container, T>::value, "make_container only accept container types.");

	return Container::make_container<T>(std::forward<Args>(args)...);
}

}  // namespace kgr
