#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

#![allow(dead_code)]

mod prelude {
  pub mod bind_ceed {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
  }
}

use std::mem;
use std::fmt;
use std::ffi::CString;
// use std::io::{self, Write};
use crate::prelude::*;

mod vector;
mod elem_restriction;
mod basis;
mod qfunction;
mod operator;

/// Ceed context wrapper
pub struct Ceed {
  backend : String,
  // Pointer to C object
  ceed_ptr : bind_ceed::Ceed,
}

/// Display
impl fmt::Display for Ceed {
  fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
    write!(f, "{}", self.backend)
  }
}

/// Destructor
impl Drop for Ceed {
  fn drop(&mut self) {
    unsafe {
      bind_ceed::CeedDestroy(&mut self.ceed_ptr);
    }
  }
}

#[derive(Clone,Copy)]
pub enum MemType {
  Host,
  Device,
}

#[derive(Clone,Copy)]
pub enum CopyMode {
  CopyValues,
  UsePointer,
  OwnPointer,
}

// Object constructors
impl Ceed {
  /// Returns a Ceed context initalized with the specified resource
  ///
  /// # arguments
  ///
  /// * 'resource' - Resource to use, e.g., "/cpu/self"
  /// 
  /// ```
  /// let ceed = ceed::Ceed::init("/cpu/self/ref/serial");
  /// ``` 
  pub fn init(resource: &str) -> Self {
    // Convert to C string
    let c_resource = CString::new(resource).expect("CString::new failed");
    
    // Call to libCEED
    let mut ceed_ptr = unsafe {libc::malloc(mem::size_of::<bind_ceed::Ceed>()) as bind_ceed::Ceed};
    unsafe { bind_ceed::CeedInit(c_resource.as_ptr() as *const i8, &mut ceed_ptr) };
    Ceed { backend : resource.to_string(), ceed_ptr: ceed_ptr }
  }
  
  /// Vector
  pub fn vector(&self, n: i32) -> crate::vector::Vector {
    crate::vector::Vector::create( self, n )
  }
  
  /// Elem Restriction
  pub fn elem_restriction(&self, nelem : i32, elemsize : i32, ncomp : i32, 
    compstride : i32, lsize : i32, mtype : MemType, cmode : CopyMode,
    offsets : &Vec<i32>) -> crate::elem_restriction::ElemRestriction {
    todo!()
  }
    
  /// Basis
  pub fn basis_tensor_H1(&self, dim: i32, ncomp: i32, P1d: i32,
    Q1d: i32, interp1d: Vec<u32> , grad1d: &Vec<u32>, qref1d: &Vec<u32>,
    qweight1d: &Vec<u32>) -> crate::basis::Basis {
    todo!()
  }
      
  /// QFunction
  pub fn q_function_interior(&self, vlength: i32, f: bind_ceed::CeedQFunctionUser,
    source: impl Into<String>) -> crate::qfunction::QFunction {
    todo!()
  }
  
  /// Operator
  pub fn operator(&self,
    qf: &crate::qfunction::QFunction,
    dqf: &crate::qfunction::QFunction,
    dqfT: &crate::qfunction::QFunction) -> crate::operator::Operator {
    todo!()
  }
}

#[cfg(test)]
mod tests {
  use super::*;
  
  #[test]
  fn ceed_t000() {
    let ceed = Ceed::init("/cpu/self/ref/serial");
    println!("{}", ceed);
    /*
    unsafe {
      let mut ceed: bind_ceed::Ceed = libc::malloc(mem::size_of::<bind_ceed::Ceed>()) as bind_ceed::Ceed;
      let resource = "/cpu/self/ref/serial";
      bind_ceed::CeedInit(resource.as_ptr() as *const i8, &mut  ceed);
      bind_ceed::CeedDestroy(&mut ceed);
    }
    */
  }
  
  fn ceed_t001() {
    let ceed = Ceed::init("/cpu/self/ref/serial");
    let vec = ceed.vector(10);
  }
}