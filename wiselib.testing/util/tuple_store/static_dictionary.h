/***************************************************************************
 ** This file is part of the generic algorithm library Wiselib.           **
 ** Copyright (C) 2008,2009 by the Wisebed (www.wisebed.eu) project.      **
 **                                                                       **
 ** The Wiselib is free software: you can redistribute it and/or modify   **
 ** it under the terms of the GNU Lesser General Public License as        **
 ** published by the Free Software Foundation, either version 3 of the    **
 ** License, or (at your option) any later version.                       **
 **                                                                       **
 ** The Wiselib is distributed in the hope that it will be useful,        **
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of        **
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         **
 ** GNU Lesser General Public License for more details.                   **
 **                                                                       **
 ** You should have received a copy of the GNU Lesser General Public      **
 ** License along with the Wiselib.                                       **
 ** If not, see <http://www.gnu.org/licenses/>.                           **
 ***************************************************************************/

#ifndef STATIC_DICTIONARY_H
#define STATIC_DICTIONARY_H

#include <external_interface/external_interface.h>
#include <external_interface/external_interface_testing.h>
#include <util/meta.h>
#include <algorithms/hash/bernstein.h>
#include <util/standalone_math.h>
#include <util/string_util.h>

namespace wiselib {
	
	/**
	 * @brief
	 * 
	 * Memory consumption will be rhoughly
	 * SLOTS * (SLOT_WIDTH + 2)
	 *
	 */
	template<
		typename OsModel_P,
		int P_SLOTS = 100,
		int P_SLOT_WIDTH = 15,
		typename Debug_P = typename OsModel_P::Debug
	>
	class StaticDictionary {

		public:
			typedef OsModel_P OsModel;
			typedef typename OsModel::block_data_t block_data_t;
			typedef typename OsModel::size_t size_type;
			typedef StandaloneMath<OsModel> Math;
			typedef Debug_P Debug;

			enum { SLOTS = P_SLOTS, SLOT_WIDTH = P_SLOT_WIDTH };
		
			//typedef typename SmallUint<SLOTS + 1>::t key_type;
			typedef ::uint8_t key_type;
			typedef block_data_t* mapped_type;

			typedef ::uint8_t refcount_t;

			typedef Bernstein<OsModel, ::uint8_t> Hash;

			enum { SUCCESS = OsModel::SUCCESS, ERR_UNSPEC = OsModel::ERR_UNSPEC };
			enum { NULL_KEY = (key_type)(-1) };
			enum { ABSTRACT_KEYS = true };

			/// Strings of this length can be represented with one metaslot
			enum { SIMPLE_STRING_LENGTH = P_SLOT_WIDTH * P_SLOT_WIDTH };

			struct Slot {
				refcount_t refcount_;
				//key_type next_;
				block_data_t data_[SLOT_WIDTH];
			};
		
			void init() {
				//strncpy(reinterpret_cast<char*>(slots_[0].data_), "<http://www.", SLOT_WIDTH);
				//slots_[0].refcount_ = 1;

				for(key_type k = 0; k < SLOTS; k++) { slots_[k].refcount_ = 0; }
			}

			template<typename Debug>
			void init(Debug* dbg) {
				debug_ = dbg;
				init();
			}

			key_type insert(mapped_type v) {
				size_type l = strlen((char*)v) + 1;

				assert(l <= SIMPLE_STRING_LENGTH);

				block_data_t *p = v;
				block_data_t *end = p + l;

				// Insert parts and create meta slot in s

				Slot s;
				make_meta(s);

				int i = 0;
				for( ; p < end; p += SLOT_WIDTH, i++) {
					bool found;
					key_type x = find_slot(Math::template min<long>(end - p, SLOT_WIDTH), p, found);
					assert(x != NULL_KEY);

					if(!found) {
						slots_[x].refcount_ = 1;
						mystrncpy(reinterpret_cast<char*>(slots_[x].data_), reinterpret_cast<char*>(p), SLOT_WIDTH);
					}
					s.data_[i] = x;
				}

				// See if this exact meta slot is already there.
				// If not, find a free slot and put it there.

				bool found;
				key_type x = find_slot(i, s.data_, found);

				if(!found) {
					slots_[x] = s;
				}
				return x;
			}

			mapped_type get_value(key_type k) {
				size_type len = 0;
				for(len = 0; len < SLOT_WIDTH; len++) {
					if(slots_[k].data_[len] == NULL_KEY) { break; }
				}

				//block_data_t *d = ::get_allocator().template allocate_array<block_data_t>(len * SLOT_WIDTH);
				key_type i = 0;
				for(; i<SLOT_WIDTH && slots_[k].data_[i] != NULL_KEY; i++) {
					memcpy(buffer_ + i * SLOT_WIDTH, slots_[i].data_, SLOT_WIDTH);
				}
				buffer_[i * SLOT_WIDTH] = 0;
				return buffer_;
			}
			block_data_t buffer_[SLOT_WIDTH * SLOT_WIDTH + 1];

			void free_value(mapped_type v) {
				//::get_allocator().template free_array(v);
				buffer_[0] = 0;
			}

			size_type count(key_type k) { return slots_[k].refcount_; }

			key_type find(mapped_type v) {
				size_type l = strlen((char*)v) + 1;

				assert(l <= SIMPLE_STRING_LENGTH);

				block_data_t *p = v;
				block_data_t *end = p + l;

				// Find parts and create meta slot in s

				Slot s;
				make_meta(s);

				int i = 0;
				for( ; p < end; p += SLOT_WIDTH, i++) {
					bool found;
					key_type x = find_slot(Math::template min<long>(end - p, SLOT_WIDTH), p, found);
					if(!found) {
						// Well, if there is a part of the string we don't
						// have, we can't have the whole string, can we?
						return NULL_KEY;
					}
					s.data_[i] = x;
				}

				// See if this exact meta slot is already there.
				// If not, find a free slot and put it there.

				bool found;
				key_type x = find_slot(i, s.data_, found);

				if(!found) { return NULL_KEY; }
				return x;
			}

			size_type erase(key_type k) {
				if(slots_[k].refcount_ == 0) { return 0; }

				// delete all referenced substrings, too (or decrease
				// their refcount at least)
				for(int i = 0; slots_[k].data_[i] != NULL_KEY && i < SLOT_WIDTH; i++) {
					erase(slots_[k].data_[i]);
				}
				slots_[k].refcount_--;
				return 1;
			}

			void debug() {
				for(key_type k = 0; k<SLOTS; k++) {
					if(slots_[k].refcount_) {
						char str[100];
						char dec[400];
						int i = 0;
						for(; i<SLOT_WIDTH; i++) {
							char x = slots_[k].data_[i];
							if(is_printable(x)) { str[i] = x; }
							else { str[i] = '.'; }

							snprintf(dec + 4*i, 5, "%3d ", (int)x);
						}
						str[i] = '\0';
						dec[4 * i] = '\0';

						debug_->debug("[%3d] x%2d: %s %s", (int)k, (int)slots_[k].refcount_,
								str, dec);
					}
				}
			}

		private:

			void make_meta(Slot& s) {
				mymemset(s.data_, NULL_KEY, sizeof(s.data_));
				//s.next_ = NULL_KEY;
				s.refcount_ = 1;
			}

			/**
			 * Return key for a slot containing the given data, if not found,
			 * return key for an unused slot.
			 */
			key_type find_slot(size_type l, block_data_t *data, bool &found) {
				key_type start_pos = Hash::hash(data, l) % SLOTS;
				key_type end_pos = start_pos ? (start_pos - 1) : (SLOTS - 1);
				key_type free = NULL_KEY;

				for(key_type i = start_pos; i != end_pos; i = (i+1) % SLOTS) {
					if(slots_[i].refcount_) {
						if(mymemcmp(data, slots_[i].data_, l) == 0) {
							// a used slot that looks like s!
							slots_[i].refcount_++;
							found = true;
							return i;
						}
					}
					else if(free == NULL_KEY) { free = i; }
				}
				
				found = false;
				return free;
			}


			//key_type find_free() {
				//for(key_type i = 0; i < SLOTS; i++) {
					//if(slots_[i].refcount_ == 0) { return i; }
				//}
				//return NULL_KEY;
			//}

			Slot slots_[SLOTS];
			typename Debug::self_pointer_t debug_;
		
	}; // StaticDictionary
}

#endif // STATIC_DICTIONARY_H
