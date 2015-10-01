/*
 *  iaf_psc_alpha_multisynapse.h
 *
 *  This file is part of NEST.
 *
 *  Copyright (C) 2004 The NEST Initiative
 *
 *  NEST is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  NEST is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with NEST.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef IAF_PSC_ALPHA_MULTISYNAPSE_H
#define IAF_PSC_ALPHA_MULTISYNAPSE_H

#include "nest_types.h"
#include "event.h"
#include "archiving_node.h"
#include "ring_buffer.h"
#include "connection.h"
#include "universal_data_logger.h"
#include "recordables_map.h"

/* BeginDocumentation
Name: iaf_psc_alpha_multisynapse - Leaky integrate-and-fire neuron model with multiple ports.

Description:

iaf_psc_alpha_multisynapse is a direct extension of iaf_psc_alpha.
On the postsynapic side, there can be arbitrarily many synaptic
time constants (iaf_psc_alpha has exactly two: tau_syn_ex and tau_syn_in).

This can be reached by specifying separate receptor ports, each for
a different time constant. The port number has to match the respective
"receptor_type" in the connectors.

Sends: SpikeEvent

Receives: SpikeEvent, CurrentEvent, DataLoggingRequest

Author:  Schrader, adapted from iaf_psc_alpha
SeeAlso: iaf_psc_alpha, iaf_psc_delta, iaf_psc_exp, iaf_cond_exp, iaf_psc_exp_multisynapse
*/

namespace nest
{
class Network;

/**
 * Leaky integrate-and-fire neuron with alpha-shaped PSCs.
 */
class iaf_psc_alpha_multisynapse : public Archiving_Node
{

public:
  iaf_psc_alpha_multisynapse();
  iaf_psc_alpha_multisynapse( const iaf_psc_alpha_multisynapse& );

  /**
   * Import sets of overloaded virtual functions.
   * @see Technical Issues / Virtual Functions: Overriding, Overloading, and Hiding
   */
  using Node::handle;
  using Node::handles_test_event;

  port send_test_event( Node&, rport, synindex, bool );

  void handle( SpikeEvent& );
  void handle( CurrentEvent& );
  void handle( DataLoggingRequest& );

  port handles_test_event( SpikeEvent&, rport );
  port handles_test_event( CurrentEvent&, rport );
  port handles_test_event( DataLoggingRequest&, rport );

  void get_status( DictionaryDatum& ) const;
  void set_status( const DictionaryDatum& );

private:
  void init_state_( const Node& proto );
  void init_buffers_();
  void calibrate();

  void update( Time const&, const long_t, const long_t );

  // The next two classes need to be friends to access the State_ class/member
  friend class RecordablesMap< iaf_psc_alpha_multisynapse >;
  friend class UniversalDataLogger< iaf_psc_alpha_multisynapse >;

  // ----------------------------------------------------------------

  /**
   * Independent parameters of the model.
   */
  struct Parameters_
  {

    /** Membrane time constant in ms. */
    double_t Tau_;

    /** Membrane capacitance in pF. */
    double_t C_;

    /** Refractory period in ms. */
    double_t TauR_;

    /** Resting potential in mV. */
    double_t U0_;

    /** External current in pA */
    double_t I_e_;

    /** Reset value of the membrane potential */
    double_t V_reset_;

    /** Threshold, RELATIVE TO RESTING POTENTIAL(!).
        I.e. the real threshold is (U0_+Theta_). */
    double_t Theta_;

    /** Lower bound, RELATIVE TO RESTING POTENTIAL(!).
        I.e. the real lower bound is (LowerBound_+Theta_). */
    double_t LowerBound_;

    /** Time constants of synaptic currents in ms. */
    std::vector< double_t > tau_syn_;

    // type is long because other types are not put through in GetStatus
    std::vector< long > receptor_types_;
    size_t num_of_receptors_;

    // boolean flag which indicates whether the neuron has connections
    bool has_connections_;

    Parameters_(); //!< Sets default parameter values

    void get( DictionaryDatum& ) const; //!< Store current values in dictionary

    /** Set values from dictionary.
     * @returns Change in reversal potential E_L, to be passed to State_::set()
     */
    double set( const DictionaryDatum& );
  }; // Parameters_

  // ----------------------------------------------------------------

  /**
   * State variables of the model.
   */
  struct State_
  {
    double_t y0_; //!< Constant current
    std::vector< double_t > y1_syn_;
    std::vector< double_t > y2_syn_;
    double_t y3_;      //!< This is the membrane potential RELATIVE TO RESTING POTENTIAL.
    double_t current_; //! This is the current in a time step. This is only here to allow logging

    int_t r_; //!< Number of refractory steps remaining

    State_(); //!< Default initialization

    void get( DictionaryDatum&, const Parameters_& ) const;

    /** Set values from dictionary.
     * @param dictionary to take data from
     * @param current parameters
     * @param Change in reversal potential E_L specified by this dict
     */
    void set( const DictionaryDatum&, const Parameters_&, const double );
  }; // State_

  // ----------------------------------------------------------------

  /**
   * Buffers of the model.
   */
  struct Buffers_
  {
    Buffers_( iaf_psc_alpha_multisynapse& );
    Buffers_( const Buffers_&, iaf_psc_alpha_multisynapse& );

    /** buffers and sums up incoming spikes/currents */
    std::vector< RingBuffer > spikes_;
    RingBuffer currents_;

    //! Logger for all analog data
    UniversalDataLogger< iaf_psc_alpha_multisynapse > logger_;
  };

  // ----------------------------------------------------------------

  /**
   * Internal variables of the model.
   */
  struct Variables_
  {
    std::vector< double_t > PSCInitialValues_;
    int_t RefractoryCounts_;

    std::vector< double_t > P11_syn_;
    std::vector< double_t > P21_syn_;
    std::vector< double_t > P22_syn_;
    std::vector< double_t > P31_syn_;
    std::vector< double_t > P32_syn_;

    double_t P30_;
    double_t P33_;

    unsigned int receptor_types_size_;

  }; // Variables

  // Access functions for UniversalDataLogger -------------------------------

  //! Read out the real membrane potential
  double_t
  get_V_m_() const
  {
    return S_.y3_ + P_.U0_;
  }
  double_t
  get_current_() const
  {
    return S_.current_;
  }

  // Data members -----------------------------------------------------------

  /**
   * @defgroup iaf_psc_alpha_multisynapse_data
   * Instances of private data structures for the different types
   * of data pertaining to the model.
   * @note The order of definitions is important for speed.
   * @{
   */
  Parameters_ P_;
  State_ S_;
  Variables_ V_;
  Buffers_ B_;
  /** @} */

  //! Mapping of recordables names to access functions
  static RecordablesMap< iaf_psc_alpha_multisynapse > recordablesMap_;
};

inline port
iaf_psc_alpha_multisynapse::send_test_event( Node& target, rport receptor_type, synindex, bool )
{
  SpikeEvent e;
  e.set_sender( *this );

  return target.handles_test_event( e, receptor_type );
}

inline port
iaf_psc_alpha_multisynapse::handles_test_event( CurrentEvent&, rport receptor_type )
{
  if ( receptor_type != 0 )
    throw UnknownReceptorType( receptor_type, get_name() );
  return 0;
}

inline port
iaf_psc_alpha_multisynapse::handles_test_event( DataLoggingRequest& dlr, rport receptor_type )
{
  if ( receptor_type != 0 )
    throw UnknownReceptorType( receptor_type, get_name() );
  return B_.logger_.connect_logging_device( dlr, recordablesMap_ );
}

inline void
iaf_psc_alpha_multisynapse::get_status( DictionaryDatum& d ) const
{
  P_.get( d );
  S_.get( d, P_ );
  Archiving_Node::get_status( d );

  ( *d )[ names::recordables ] = recordablesMap_.get_list();
}

inline void
iaf_psc_alpha_multisynapse::set_status( const DictionaryDatum& d )
{
  Parameters_ ptmp = P_;                 // temporary copy in case of errors
  const double delta_EL = ptmp.set( d ); // throws if BadProperty
  State_ stmp = S_;                      // temporary copy in case of errors
  stmp.set( d, ptmp, delta_EL );         // throws if BadProperty

  // We now know that (ptmp, stmp) are consistent. We do not
  // write them back to (P_, S_) before we are also sure that
  // the properties to be set in the parent class are internally
  // consistent.
  Archiving_Node::set_status( d );

  // if we get here, temporaries contain consistent set of properties
  P_ = ptmp;
  S_ = stmp;
}

} // namespace

#endif /* #ifndef IAF_PSC_ALPHA_MULTISYNAPSE_H */
