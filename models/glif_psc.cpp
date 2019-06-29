/*
 *  glif_psc.cpp
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

#include "glif_psc.h"

// C++ includes:
#include <limits>
#include <iostream>

// Includes from libnestutil:
#include "numerics.h"
#include "propagator_stability.h"

// Includes from nestkernel:
#include "exceptions.h"
#include "kernel_manager.h"
#include "universal_data_logger_impl.h"
#include "name.h"

// Includes from sli:
#include "dict.h"
#include "dictutils.h"
#include "doubledatum.h"
#include "integerdatum.h"
#include "lockptrdatum.h"

using namespace nest;

nest::RecordablesMap< nest::glif_psc > nest::glif_psc::recordablesMap_;

namespace nest
{
// Override the create() method with one call to RecordablesMap::insert_()
// for each quantity to be recorded.
template <>
void
RecordablesMap< nest::glif_psc >::create()
{
  insert_( names::V_m, &nest::glif_psc::get_V_m_ );
  insert_( Name( "AScurrents_sum" ), &nest::glif_psc::get_AScurrents_sum_ );
  insert_( names::I_syn, &nest::glif_psc::get_I_syn_ );
}
}

/* ----------------------------------------------------------------
 * Default constructors defining default parameters and state
 * ---------------------------------------------------------------- */

nest::glif_psc::Parameters_::Parameters_()
  : E_L_( -78.85 )            // in mV
  , G_( 9.43 )                // in nS
  , th_inf_( 27.17 )          // in mv, rel to E_L_, 51.68 - E_L_
  , C_m_( 58.72 )             // in pF
  , t_ref_( 3.75 )            // in ms
  , V_reset_( 0.0 )           // in mV, rel to E_L_, -78.85 - E_L_
  , a_spike_( 0.37 )          // in mV
  , b_spike_( 0.009 )         // in 1/ms
  , voltage_reset_a_( 0.20 )  // deterministic
  , voltage_reset_b_( 18.51 ) // in mV
  , a_voltage_( 0.005 )       // in 1/ms
  , b_voltage_( 0.09 )        // in 1/ms
  , asc_init_( std::vector< double >( 2, 0.0 ) )         // in pA
  , k_( std::vector< double >{ 0.003, 0.1 } )            // in 1/ms
  , asc_amps_( std::vector< double >{ -9.18, -198.94 } ) // in pA
  , r_( std::vector< double >( 2, 1.0 ) )                // deterministic
  , tau_syn_( 1, 2.0 )                                   // ms
  , has_connections_( false )
  , glif_model_( "lif" )
{
}

nest::glif_psc::State_::State_( const Parameters_& p )
  : U_( 0.0 )                                      // in mV
  , ASCurrents_( std::vector< double >( 2, 0.0 ) ) // in pA
  , threshold_( -51.68 - p.E_L_ )                  // in mV
  , I_( 0.0 )                                      // in pA
{
  y1_.clear();
  y2_.clear();
  for ( std::size_t a = 0; a < ASCurrents_.size(); ++a )
  {
    ASCurrents_[ a ] = p.asc_init_[ a ];
  }
}

/* ----------------------------------------------------------------
 * Parameter and state extractions and manipulation functions
 * ---------------------------------------------------------------- */

void
nest::glif_psc::Parameters_::get( DictionaryDatum& d ) const
{
  def< double >( d, names::V_th, th_inf_ + E_L_ );
  def< double >( d, names::g, G_ );
  def< double >( d, names::E_L, E_L_ );
  def< double >( d, names::C_m, C_m_ );
  def< double >( d, names::t_ref, t_ref_ );
  def< double >( d, names::V_reset, V_reset_ + E_L_ );

  def< double >( d, names::a_spike, a_spike_ );
  def< double >( d, names::b_spike, b_spike_ );
  def< double >( d, names::a_reset, voltage_reset_a_ );
  def< double >( d, names::b_reset, voltage_reset_b_ );

  def< double >( d, names::a_voltage, a_voltage_ );
  def< double >( d, names::b_voltage, b_voltage_ );

  def< std::vector< double > >( d, names::asc_init, asc_init_ );
  def< std::vector< double > >( d, names::k, k_ );
  def< std::vector< double > >( d, names::asc_amps, asc_amps_ );
  def< std::vector< double > >( d, names::r, r_ );
  ArrayDatum tau_syn_ad( tau_syn_ );
  def< ArrayDatum >( d, names::tau_syn, tau_syn_ad );
  def< bool >( d, names::has_connections, has_connections_ );
  def< model_type >( d, names::glif_model, glif_model_ );
}

double
nest::glif_psc::Parameters_::set( const DictionaryDatum& d )
{
  // if E_L_ is changed, we need to adjust all variables defined relative to
  // E_L_
  const double ELold = E_L_;
  updateValue< double >( d, names::E_L, E_L_ );
  const double delta_EL = E_L_ - ELold;

  if ( updateValue< double >( d, names::V_reset, V_reset_ ) )
  {
    V_reset_ -= E_L_;
  }
  else
  {
    V_reset_ -= delta_EL;
  }

  if ( updateValue< double >( d, names::V_th, th_inf_ ) )
  {
    th_inf_ -= E_L_;
  }
  else
  {
    th_inf_ -= delta_EL;
  }

  updateValue< double >( d, names::g, G_ );
  updateValue< double >( d, names::C_m, C_m_ );
  updateValue< double >( d, names::t_ref, t_ref_ );

  updateValue< double >( d, names::a_spike, a_spike_ );
  updateValue< double >( d, names::b_spike, b_spike_ );
  updateValue< double >( d, names::a_reset, voltage_reset_a_ );
  updateValue< double >( d, names::b_reset, voltage_reset_b_ );

  updateValue< double >( d, names::a_voltage, a_voltage_ );
  updateValue< double >( d, names::b_voltage, b_voltage_ );

  updateValue< std::vector< double > >( d, names::asc_init, asc_init_ );
  updateValue< std::vector< double > >( d, names::k, k_ );
  updateValue< std::vector< double > >( d, names::asc_amps, asc_amps_ );
  updateValue< std::vector< double > >( d, names::r, r_ );
  updateValue< std::vector< double > >( d, names::tau_syn, tau_syn_ );
  updateValue< model_type >( d, names::glif_model, glif_model_ );

  if ( V_reset_ >= th_inf_ )
  {
    throw BadProperty( "Reset potential must be smaller than threshold." );
  }

  if ( C_m_ <= 0.0 )
  {
    throw BadProperty( "Capacitance must be strictly positive." );
  }

  if ( G_ <= 0.0 )
  {
    throw BadProperty( "Membrane conductance must be strictly positive." );
  }

  if ( t_ref_ <= 0.0 )
  {
    throw BadProperty( "Refractory time constant must be strictly positive." );
  }

  if ( b_voltage_ <= 0.0 )
  {
    throw BadProperty(
      "Voltage-induced threshold time constant must be strictly positive." );
  }

  if ( b_spike_ <= 0.0 )
  {
    throw BadProperty(
      "Spike induced threshold time constant must be strictly positive." );
  }

  for ( std::size_t a = 0; a < k_.size(); ++a )
  {
    if ( k_[ a ] <= 0.0 )
    {
      throw BadProperty(
        "After-spike current time constant must be strictly positive." );
    }
  }

  const size_t old_n_receptors = this->n_receptors_();
  if ( updateValue< std::vector< double > >( d, names::tau_syn, tau_syn_ ) )
  {
    if ( this->n_receptors_() != old_n_receptors && has_connections_ == true )
    {
      throw BadProperty(
        "The neuron has connections, therefore the number of ports cannot be "
        "reduced." );
    }
    for ( size_t i = 0; i < tau_syn_.size(); ++i )
    {
      if ( tau_syn_[ i ] <= 0 )
      {
        throw BadProperty(
          "All synaptic time constants must be strictly positive." );
      }
    }
  }
  return delta_EL;
}

void
nest::glif_psc::State_::get( DictionaryDatum& d, const Parameters_& p ) const
{
  def< double >( d, names::V_m, U_ + p.E_L_ );
  def< std::vector< double > >( d, names::ASCurrents, ASCurrents_ );
}

void
nest::glif_psc::State_::set( const DictionaryDatum& d,
  const Parameters_& p,
  double delta_EL )
{
  // updateValue< double >( d, names::V_m, V_m_ );
  if ( updateValue< double >( d, names::V_m, U_ ) )
  {
    U_ -= p.E_L_;
  }
  else
  {
    U_ -= delta_EL;
  }

  updateValue< std::vector< double > >( d, names::ASCurrents, ASCurrents_ );
}

nest::glif_psc::Buffers_::Buffers_( glif_psc& n )
  : logger_( n )
{
}

nest::glif_psc::Buffers_::Buffers_( const Buffers_&, glif_psc& n )
  : logger_( n )
{
}

/* ----------------------------------------------------------------
 * Default and copy constructor for node
 * ---------------------------------------------------------------- */

nest::glif_psc::glif_psc()
  : Archiving_Node()
  , P_()
  , S_( P_ )
  , B_( *this )
{
  recordablesMap_.create();
}

nest::glif_psc::glif_psc( const glif_psc& n )
  : Archiving_Node( n )
  , P_( n.P_ )
  , S_( n.S_ )
  , B_( n.B_, *this )
{
}

/* ----------------------------------------------------------------
 * Node initialization functions
 * ---------------------------------------------------------------- */

void
nest::glif_psc::init_state_( const Node& proto )
{
  const glif_psc& pr = downcast< glif_psc >( proto );
  S_ = pr.S_;
}

void
nest::glif_psc::init_buffers_()
{
  B_.spikes_.clear();   // includes resize
  B_.currents_.clear(); // include resize
  B_.logger_.reset();   // includes resize
}

void
nest::glif_psc::calibrate()
{
  B_.logger_.init();

  V_.t_ref_remaining_ = 0.0;
  V_.t_ref_total_ = P_.t_ref_;
  V_.last_spike_ = 0.0;
  V_.last_voltage_ = 0.0;

  // post synapse currents
  const double h = Time::get_resolution().get_ms(); // in ms

  V_.P11_.resize( P_.n_receptors_() );
  V_.P21_.resize( P_.n_receptors_() );
  V_.P22_.resize( P_.n_receptors_() );
  V_.P31_.resize( P_.n_receptors_() );
  V_.P32_.resize( P_.n_receptors_() );

  S_.y1_.resize( P_.n_receptors_() );
  S_.y2_.resize( P_.n_receptors_() );
  V_.PSCInitialValues_.resize( P_.n_receptors_() );

  B_.spikes_.resize( P_.n_receptors_() );

  double Tau_ = P_.C_m_ / P_.G_; // in ms
  V_.P33_ = std::exp( -h / Tau_ );
  V_.P30_ = 1 / P_.C_m_ * ( 1 - V_.P33_ ) * Tau_;

  for ( size_t i = 0; i < P_.n_receptors_(); i++ )
  {
    // these P are independent
    V_.P11_[ i ] = V_.P22_[ i ] = std::exp( -h / P_.tau_syn_[ i ] );

    V_.P21_[ i ] = h * V_.P11_[ i ];

    // these are determined according to a numeric stability criterion
    // input time parameter shall be in ms, capacity in pF
    V_.P31_[ i ] = propagator_31( P_.tau_syn_[ i ], Tau_, P_.C_m_, h );
    V_.P32_[ i ] = propagator_32( P_.tau_syn_[ i ], Tau_, P_.C_m_, h );

    V_.PSCInitialValues_[ i ] = 1.0 * numerics::e / P_.tau_syn_[ i ];
    B_.spikes_[ i ].resize();
  }
}

/* ----------------------------------------------------------------
 * Update and spike handling functions
 * ---------------------------------------------------------------- */

void
nest::glif_psc::update( Time const& origin, const long from, const long to )
{
  // glif_lif_r_asc_a
  const double dt = Time::get_resolution().get_ms();

  // get model type
  std::string model_str = P_.glif_model_;
  std::transform(
    model_str.begin(), model_str.end(), model_str.begin(), ::tolower );
  if ( nest::glif_psc::model_type_lu.find( model_str )
    == nest::glif_psc::model_type_lu.end() )
  {
    throw BadProperty( "Bad glif model type string." );
  }
  long model_type = nest::glif_psc::model_type_lu[ model_str ];

  double v_old = S_.U_;
  double spike_component = 0.0;
  double voltage_component = 0.0;
  double th_old = S_.threshold_;

  for ( long lag = from; lag < to; ++lag )
  {

    // update threshold via exact solution of dynamics of spike component of
    // threshold
    // exact solution of dynamics of spike component of threshold for glif2/4/5
    // models with "R"
    if ( model_type == 2 || model_type == 4 || model_type == 5 )
    {
      spike_component = V_.last_spike_ * std::exp( -P_.b_spike_ * dt );
    }
    V_.last_spike_ = spike_component;

    if ( V_.t_ref_remaining_ > 0.0 )
    {
      // While neuron is in refractory period count-down in time steps (since dt
      // may change while in refractory) while holding the voltage at last peak.
      V_.t_ref_remaining_ -= dt;
      if ( V_.t_ref_remaining_ <= 0.0 )
      {
        // Neuron has left refractory period, reset voltage and after-spike
        // current

        // Reset ASC_currentsfor glif3/4/5 models with "ASC"
        if ( model_type == 3 || model_type == 4 || model_type == 5 )
        {
          for ( std::size_t a = 0; a < S_.ASCurrents_.size(); ++a )
          {
            S_.ASCurrents_[ a ] = P_.asc_amps_[ a ]
              + S_.ASCurrents_[ a ] * P_.r_[ a ]
                * std::exp( -P_.k_[ a ] * V_.t_ref_total_ );
          }
        }

        // Reset voltage
        if ( model_type == 1 || model_type == 3 )
        {
          // Reset voltage for glif1/3 models without "R"
          S_.U_ = P_.V_reset_;
        }
        else
        {
          // Reset voltage for glif2/4/5 models with "R"
          S_.U_ = P_.voltage_reset_a_ * S_.U_ + P_.voltage_reset_b_;

          // reset spike component of threshold
          V_.last_spike_ = V_.last_spike_ + P_.a_spike_;

          // rest the global threshold (voltage component of threshold: stay the
          // same)
          S_.threshold_ = V_.last_spike_ + V_.last_voltage_ + P_.th_inf_;
        }
        // Check if bad reset
        // TODO: Better way to handle?
        if ( S_.U_ > S_.threshold_ )
        {
          printf(
            "Simulation Terminated: Voltage (%f) reset above threshold "
            "(%f)!!\n",
            S_.U_,
            S_.threshold_ );
        }
        assert( S_.U_ <= S_.threshold_ );
      }
      else
      {
        S_.U_ = v_old;
      }
    }
    else
    {
      // Integrate voltage and currents

      // Calculate new ASCurrents value using expoential methods
      S_.ASCurrents_sum_ = 0.0;
      // for glif3/4/5 models with "ASC"
      if ( model_type == 3 || model_type == 4 || model_type == 5 )
      {
        for ( std::size_t a = 0; a < S_.ASCurrents_.size(); ++a )
        {
          S_.ASCurrents_sum_ += S_.ASCurrents_[ a ];
          S_.ASCurrents_[ a ] =
            S_.ASCurrents_[ a ] * std::exp( -P_.k_[ a ] * dt );
        }
      }
      // voltage dynamics of membranes
      // Linear Exact to find next V_m value
      S_.U_ = v_old * V_.P33_ + ( S_.I_ + S_.ASCurrents_sum_ ) * V_.P30_;

      // add synapse component for voltage dynamics
      S_.I_syn_ = 0.0;
      for ( size_t i = 0; i < P_.n_receptors_(); i++ )
      {
        S_.U_ += V_.P31_[ i ] * S_.y1_[ i ] + V_.P32_[ i ] * S_.y2_[ i ];
        S_.I_syn_ += S_.y2_[ i ];
      }

      // Calculate exact voltage component of the threshold for glif5 model with
      // "A"
      if ( model_type == 5 )
      {
        double beta = ( S_.I_ + S_.ASCurrents_sum_ ) / P_.G_;
        double phi = P_.a_voltage_ / ( P_.b_voltage_ - P_.G_ / P_.C_m_ );
        voltage_component =
          phi * ( v_old - beta ) * std::exp( -P_.G_ * dt / P_.C_m_ )
          + 1 / ( std::exp( P_.b_voltage_ * dt ) )
            * ( V_.last_voltage_ - phi * ( v_old - beta )
                - ( P_.a_voltage_ / P_.b_voltage_ ) * ( beta ) )
          + ( P_.a_voltage_ / P_.b_voltage_ ) * ( beta );
      }

      S_.threshold_ = V_.last_spike_ + voltage_component + P_.th_inf_;
      V_.last_voltage_ = voltage_component;

      // Check if their is an action potential
      if ( S_.U_ > S_.threshold_ )
      {
        // Marks that the neuron is in a refractory period
        V_.t_ref_remaining_ = V_.t_ref_total_;

        // Find the exact time during this step that the neuron crossed the
        // threshold and record it
        double spike_offset =
          ( 1
            - ( v_old - th_old )
              / ( ( S_.threshold_ - th_old ) - ( S_.U_ - v_old ) ) )
          * Time::get_resolution().get_ms();
        set_spiketime(
          Time::step( origin.get_steps() + lag + 1 ), spike_offset );
        SpikeEvent se;
        se.set_offset( spike_offset );
        kernel().event_delivery_manager.send( *this, se, lag );
      }
    }

    // alpha shape PSCs
    for ( size_t i = 0; i < P_.n_receptors_(); i++ )
    {
      S_.y2_[ i ] = V_.P21_[ i ] * S_.y1_[ i ] + V_.P22_[ i ] * S_.y2_[ i ];
      S_.y1_[ i ] *= V_.P11_[ i ];

      // Apply spikes delivered in this step: The spikes arriving at T+1 have an
      // immediate effect on the state of the neuron
      S_.y1_[ i ] +=
        V_.PSCInitialValues_[ i ] * B_.spikes_[ i ].get_value( lag );
    }

    double cursum = 0.0;
    for ( size_t i = 0; i < P_.n_receptors_(); i++ )
    {
      cursum += S_.y1_[ i ];
    }

    // Update any external currents
    S_.I_ = B_.currents_.get_value( lag );

    // Save voltage
    B_.logger_.record_data( origin.get_steps() + lag );

    v_old = S_.U_;

    th_old = S_.threshold_;
  }
}

nest::port
nest::glif_psc::handles_test_event( SpikeEvent&, rport receptor_type )
{
  if ( receptor_type <= 0
    || receptor_type > static_cast< port >( P_.n_receptors_() ) )
  {
    throw IncompatibleReceptorType( receptor_type, get_name(), "SpikeEvent" );
  }

  P_.has_connections_ = true;
  return receptor_type;
}

void
nest::glif_psc::handle( SpikeEvent& e )
{
  assert( e.get_delay_steps() > 0 );

  B_.spikes_[ e.get_rport() - 1 ].add_value(
    e.get_rel_delivery_steps( kernel().simulation_manager.get_slice_origin() ),
    e.get_weight() * e.get_multiplicity() );
}

void
nest::glif_psc::handle( CurrentEvent& e )
{
  assert( e.get_delay_steps() > 0 );

  B_.currents_.add_value(
    e.get_rel_delivery_steps( kernel().simulation_manager.get_slice_origin() ),
    e.get_weight() * e.get_current() );
}

// Do not move this function as inline to h-file. It depends on
// universal_data_logger_impl.h being included here.
void
nest::glif_psc::handle( DataLoggingRequest& e )
{
  B_.logger_.handle( e ); // the logger does this for us
}