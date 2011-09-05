/*
 * File:   sema_chd.h
 * Author: amaxilat
 */

#ifndef __SEMA_CHD_H_
#define __SEMA_CHD_H_

#include "util//pstl/vector_static.h"

namespace wiselib {

    /**
     * Semantic cluster head decision module
     */
    template<typename OsModel_P, typename Radio_P, typename Semantics_P>
    class SemanticClusterHeadDecision {
    public:

        typedef OsModel_P OsModel;
        //TYPEDEFS
        typedef Radio_P Radio;
        //typedef typename OsModel::Radio Radio;
        typedef typename OsModel::Debug Debug;
        typedef Semantics_P Semantics_t;
        typedef typename Semantics_t::semantics_t semantics_t;
        typedef typename Semantics_t::semantics_vector_t semantics_vector_t;
        typedef typename Semantics_t::semantics_vector_iterator_t semantics_vector_iterator_t;
        // data types
        typedef typename Radio::node_id_t node_id_t;
        typedef typename Radio::size_t size_t;
        typedef typename Radio::block_data_t block_data_t;
        typedef node_id_t cluster_id_t;
        typedef int cluster_level_t; //quite useless within current scheme, supported for compatibility issues

        //        typedef wiselib::vector_static<OsModel, semantics_t, 10 > semantics_vector_t;

        // delegate
        typedef delegate1<int, int*> chd_delegate_t;

        typedef delegate1<void, int > head_delegate_t;

        /*
         * Constructor
         * */
        SemanticClusterHeadDecision() :
        cluster_head_(false) {
            head_delegate_ = head_delegate_t();
        }

        /*
         * Destructor
         * */
        ~SemanticClusterHeadDecision() {
        }

        /*
         * INIT
         * initializes the values of radio and debug
         */
        void init(Radio& radio, Debug& debug, Semantics_t &semantics) {
            radio_ = &radio;
            debug_ = &debug;
            semantics_ = &semantics;
        }

        /* SET functions */

        void set_attribute(int theta) {
            theta_ = theta;
            min_theta_ = theta;
        }

        /* GET functions */

        // Returns if Cluster Head

        inline bool is_cluster_head(void) {
            return cluster_head_;
        }

        /*
         * Reset
         * resets the module
         * initializes values
         * */
        inline void reset() {
            cluster_head_ = false;
            min_theta_ = theta_;
        }

        /*
         * CALCULATE_HEAD
         * defines if the node is a cluster head or not
         * if a cluster head return true
         * */
        inline bool calculate_head() {

            for (semantics_vector_iterator_t si = semantics_->semantics_vector_.begin(); si != semantics_->semantics_vector_.end(); ++si) {
                //                debug_->debug("semantic is %d|%d  %x", si->semantic_id_, si->semantic_value_, si->node_id_);
                if ((si->semantic_id_ <= 200) && (si->node_id_ == radio_->id()) && (si->enabled_)) {
                    //                    debug_->debug("semantic chead of c %x|%x", si->semantic_id_, si->semantic_value_);
                    cluster_head_ = true;
                } else {
                    //                    debug_->debug("NOT semantic chead of c %x|%x", si->semantic_id_, si->semantic_value_);
                }
            }
            if (cluster_head_) {
                became_head(1);
            }

            return cluster_head_;
        }

        void receive(node_id_t from, size_t len, block_data_t * mess) {
            SemaAttributeMsg_t msg;
            memcpy(&msg, mess, len);
            size_t count = msg.contained() / sizeof (semantics_t);
            semantics_t a[count];
            msg.payload((uint8_t *) a);
            //debug_->debug("received a mess with %d semantics from %d", count, radio_->id());
            bool fits = true;
            for (size_t i = 0; i < count; i++) {
                //  debug_->debug("semantic is %x  |from %x", a[i].cluster_id_, a[i].node_id_);                
                for (semantics_vector_iterator_t si = semantics_->semantics_vector_.begin(); si != semantics_->semantics_vector_.end(); ++si) {
                    if (si->semantic_id_ <= 200) {
                        if ((si->semantic_id_ == a[i].semantic_id_) && (si->semantic_value_ != a[i].semantic_value_)) {
                            //        debug_->debug("seting %x as my sh %x", si->node_id_, si->cluster_id_);
                            //                            debug_->debug("dropping from %x", from);
                            fits = false;
                        }
                    }
                }
            }
            if (fits) {
                for (size_t i = 0; i < count; i++) {
                    //  debug_->debug("semantic is %x  |from %x", a[i].cluster_id_, a[i].node_id_);                
                    for (semantics_vector_iterator_t si = semantics_->semantics_vector_.begin(); si != semantics_->semantics_vector_.end(); ++si) {
                        if ((si->node_id_ > a[i].node_id_) && (si->semantic_id_ == a[i].semantic_id_)) {
                            //                            debug_->debug("setting from %x", from);
                            si->node_id_ = a[i].node_id_;
                        }
                    }
                }
            }
        }

        SemaAttributeMsg_t get_attribute_payload() {
            //debug_->debug("adding %d semantics", semantics.size());
            SemaAttributeMsg_t msg;
            size_t count = 0;
            semantics_t a[semantics_->enabled_semantics()];
            //            debug_->debug("total semantics %d", semantics_vector_.size());
            for (semantics_vector_iterator_t si = semantics_->semantics_vector_.begin(); si != semantics_->semantics_vector_.end(); ++si) {
                if (si->enabled_) {
                    a[count].semantic_id_ = si->semantic_id_;
                    a[count].semantic_value_ = si->semantic_value_;
                    a[count++].node_id_ = si->node_id_;
                }
            }

            msg.set_payload((uint8_t *) a, count * sizeof (typename Semantics_t::semantics_t));
#ifdef DEBUG_PAYLOADS
            //debug_->debug("Payload::%x::[%d|%x]::", radio_->id(), type, theta_);
#endif
            return msg;
        }

        template<class T, void (T::*TMethod)(int) >
        int reg_became_head_callback(T * obj_pnt) {
            head_delegate_ = head_delegate_t::template from_method<T, TMethod > (obj_pnt);
            return head_delegate_;
        }
        // --------------------------------------------------------------------

        int unreg_cluster_joined_callback(int idx) {
            head_delegate_ = head_delegate_t();
            return idx;
        }
        // --------------------------------------------------------------------

        void became_head(int val) {

            if (head_delegate_ != head_delegate_t()) {
                (head_delegate_) (val);
            }
        }

    private:

        bool cluster_head_; // if a cluster head        
        node_id_t theta_; // clustering parameter
        head_delegate_t head_delegate_;
        int min_theta_;
        Semantics_t * semantics_;

        Radio * radio_;
        Debug * debug_;

    };

}

#endif