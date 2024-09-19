/*
 * WebRender.h
 *
 *  Created on: Sep 19, 2024
 *      Author: mad
 */

#ifndef INCLUDE_MMX_WEBRENDER_H_
#define INCLUDE_MMX_WEBRENDER_H_

#include <mmx/addr_t.hpp>
#include <mmx/uint128.hpp>
#include <mmx/fixed128.hpp>
#include <mmx/pubkey_t.hpp>
#include <mmx/signature_t.hpp>
#include <mmx/accept_generic.hxx>

#include <vnx/Visitor.h>
#include <vnx/Object.hpp>
#include <vnx/Variant.hpp>


namespace mmx {

template<typename T>
vnx::Object web_render(const T& value);

template<typename T>
vnx::Variant web_render(std::shared_ptr<T> value);


class WebRender {
public:
	vnx::Object object;
	vnx::Variant result;

	WebRender() = default;

	template<typename T>
	void type_begin(int num_fields) {
		if(auto type_code = T::static_get_type_code()) {
			object["__type"] = type_code->name;
		}
	}

	template<typename T>
	void type_end(int num_fields) {
		result = vnx::Variant(object);
	}

	void type_field(const std::string& name, const size_t index) {
		p_value = &object[name];
	}

	template<typename T>
	void set(const T& value) {
		*p_value = value;
	}

	template<typename T>
	void accept(const T& value) {
		if constexpr(vnx::is_object<T>()) {
			set(web_render(value));
		} else {
			set(value);
		}
	}

	template<size_t N>
	void accept(const bytes_t<N>& value) {
		set(value.to_string());
	}

	void accept(const uint128& value) {
		set(value.str(10));
	}

	void accept(const fixed128& value) {
		set(value.to_string());
	}

	void accept(const hash_t& value) {
		set(value.to_string());
	}

	void accept(const addr_t& value) {
		set(value.to_string());
	}

	void accept(const pubkey_t& value) {
		set(value.to_string());
	}

	void accept(const signature_t& value) {
		set(value.to_string());
	}

	template<typename T>
	void accept(const vnx::optional<T>& value) {
		if(value) {
			accept(*value);
		} else {
			set(nullptr);
		}
	}

	template<typename K, typename V>
	void accept(const std::pair<K, V>& value) {
		const auto prev = p_value;
		std::array<vnx::Variant, 2> tmp;
		p_value = &tmp[0];
		accept(value.first);
		p_value = &tmp[1];
		accept(value.second);
		p_value = prev;
		set(tmp);
	}

	template<typename T, size_t N>
	void accept(const std::array<T, N>& value) {
		accept_range(value.begin(), value.end());
	}

	template<typename T>
	void accept(const std::vector<T>& value) {
		accept_range(value.begin(), value.end());
	}

	template<typename T>
	void accept(const std::set<T>& value) {
		accept_range(value.begin(), value.end());
	}

	template<typename K, typename V>
	void accept(const std::map<K, V>& value) {
		accept_range(value.begin(), value.end());
	}

	template<typename T>
	void accept_range(const T& begin, const T& end) {
		const auto prev = p_value;
		std::list<vnx::Variant> tmp;
		for(T iter = begin; iter != end; ++iter) {
			tmp.emplace_back();
			p_value = &tmp.back();
			accept(*iter);
		}
		p_value = prev;
		set(tmp);
	}

	void accept(const std::vector<uint8_t>& value) {
		set(vnx::to_hex_string(value.data(), value.size()));
	}

	template<typename T>
	void accept(std::shared_ptr<T> value) {
		set(web_render(value));
	}

private:
	vnx::Variant* p_value = &result;

};


template<typename T>
vnx::Object web_render(const T& value) {
	WebRender visitor;
	value.accept_generic(visitor);
	return std::move(visitor.object);
}

template<typename T>
vnx::Variant web_render(std::shared_ptr<T> value) {
	WebRender visitor;
	vnx::accept_generic(visitor, std::shared_ptr<const T>(value));
	return std::move(visitor.result);
}

template<typename T>
vnx::Variant web_render_value(const T& value) {
	WebRender visitor;
	vnx::accept_generic(visitor, value);
	return std::move(visitor.result);
}


} // mmx

#endif /* INCLUDE_MMX_WEBRENDER_H_ */
