package com.ardash.hashcash.test;

import static org.junit.Assert.*;

import java.security.NoSuchAlgorithmException;

import org.junit.Test;

import com.nettgryppa.security.HashCash;

public class HashCashTest {

	@Test
	public void test() throws NoSuchAlgorithmException {
		//HashCash hc = new HashCash("")
		HashCash hc = HashCash.mintCash("foo@bar", 20,1);
		System.out.println(hc);
//		hc.estimateTime(value)
//		fail("Not yet implemented");
	}
	@Test
	public void verifytest() throws NoSuchAlgorithmException {
		HashCash hc = new HashCash("1:48:110416:etienne1@cri.fr:::000A2F00000063BF012");
//		System.out.println(hc.);
		
//		HashCash hc = 
		System.out.println(hc);
		
//		hc.estimateTime(value)
//		fail("Not yet implemented");
	}
	
}
